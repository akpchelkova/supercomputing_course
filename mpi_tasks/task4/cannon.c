#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size, N = 64;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    
    // проверяем что количество процессов - квадрат целого числа
    // так как алгоритм Кэннона требует квадратную сетку процессов
    int q = (int)sqrt(size);
    if (q * q != size) {
        if (rank == 0) {
            printf("Error: Number of processes must be a perfect square (1, 4, 9, 16...)\n");
            printf("Current: %d\n", size);
        }
        MPI_Finalize();
        return 1;
    }
    
    // проверяем что размер матрицы делится на размер сетки процессов
    int block_size = N / q;
    if (block_size * q != N) {
        if (rank == 0) {
            printf("Error: Matrix size %d must be divisible by %d\n", N, q);
        }
        MPI_Finalize();
        return 1;
    }
    
    double start_time = 0;
    double *A = NULL, *B = NULL, *C = NULL;
    double *local_A, *local_B, *local_C;
    
    // главный процесс создает и инициализирует матрицы
    if (rank == 0) {
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double));
        C = malloc(N * N * sizeof(double));
        
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 10.0;
            B[i] = (double)rand() / RAND_MAX * 10.0;
        }
        
        printf("Cannon Algorithm: %dx%d, %d processes (%dx%d grid)\n", N, N, size, q, q);
        start_time = MPI_Wtime();
    }
    
    // рассылаем размеры матрицы и блоков всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&block_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // выделяем память под локальные блоки матриц
    int block_elements = block_size * block_size;
    local_A = malloc(block_elements * sizeof(double));
    local_B = malloc(block_elements * sizeof(double));
    // инициализируем локальную матрицу C нулями
    local_C = calloc(block_elements, sizeof(double));
    
    // создаем декартову топологию - сетку процессов q x q
    MPI_Comm grid_comm;
    int dims[2] = {q, q};
    int periods[2] = {1, 1}; // циклическая граница для реализации циркуляции
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &grid_comm);
    
    // получаем координаты текущего процесса в сетке
    int coords[2];
    MPI_Cart_coords(grid_comm, rank, 2, coords);
    int my_i = coords[0], my_j = coords[1];
    
    // распределяем блоки матриц по процессам
    if (rank == 0) {
        // главный процесс отправляет блоки другим процессам
        for (int i = 0; i < q; i++) {
            for (int j = 0; j < q; j++) {
                if (i == 0 && j == 0) continue; // себе не отправляем
                
                int dest_rank;
                int dest_coords[2] = {i, j};
                MPI_Cart_rank(grid_comm, dest_coords, &dest_rank);
                
                // отправляем блок A[i,j] построчно
                for (int x = 0; x < block_size; x++) {
                    int src_row = i * block_size + x;
                    MPI_Send(&A[src_row * N + j * block_size], block_size, MPI_DOUBLE, 
                            dest_rank, 0, MPI_COMM_WORLD);
                }
                
                // отправляем блок B[i,j] построчно  
                for (int x = 0; x < block_size; x++) {
                    int src_row = i * block_size + x;
                    MPI_Send(&B[src_row * N + j * block_size], block_size, MPI_DOUBLE,
                            dest_rank, 1, MPI_COMM_WORLD);
                }
            }
        }
        
        // главный процесс копирует свои блоки (левый верхний угол)
        for (int x = 0; x < block_size; x++) {
            int src_row = x;
            for (int y = 0; y < block_size; y++) {
                local_A[x * block_size + y] = A[src_row * N + y];
                local_B[x * block_size + y] = B[src_row * N + y];
            }
        }
    } else {
        // остальные процессы получают свои блоки
        for (int x = 0; x < block_size; x++) {
            MPI_Recv(&local_A[x * block_size], block_size, MPI_DOUBLE, 
                    0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&local_B[x * block_size], block_size, MPI_DOUBLE,
                    0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    // начальная перестановка блоков матрицы A (сдвиг влево на i позиций)
    int left_rank, right_rank;
    MPI_Cart_shift(grid_comm, 1, -my_i, &left_rank, &right_rank);
    MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                        left_rank, 0, right_rank, 0, grid_comm, MPI_STATUS_IGNORE);
    
    // начальная перестановка блоков матрицы B (сдвиг вверх на j позиций)  
    int up_rank, down_rank;
    MPI_Cart_shift(grid_comm, 0, -my_j, &up_rank, &down_rank);
    MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                        up_rank, 0, down_rank, 0, grid_comm, MPI_STATUS_IGNORE);
    
    // основной цикл алгоритма Кэннона
    for (int step = 0; step < q; step++) {
        // локальное умножение блоков матриц
        for (int i = 0; i < block_size; i++) {
            for (int j = 0; j < block_size; j++) {
                for (int k = 0; k < block_size; k++) {
                    local_C[i * block_size + j] += 
                        local_A[i * block_size + k] * local_B[k * block_size + j];
                }
            }
        }
        
        // циркуляция блоков (кроме последней итерации)
        if (step < q - 1) {
            // сдвиг блоков A влево
            MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                                left_rank, 0, right_rank, 0, grid_comm, MPI_STATUS_IGNORE);
            // сдвиг блоков B вверх
            MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                                up_rank, 0, down_rank, 0, grid_comm, MPI_STATUS_IGNORE);
        }
    }
    
    // сбор результатов на главном процессе
    if (rank == 0) {
        // главный процесс собирает результаты
        for (int x = 0; x < block_size; x++) {
            for (int y = 0; y < block_size; y++) {
                C[x * N + y] = local_C[x * block_size + y];
            }
        }
        
        // получаем блоки от других процессов
        for (int i = 0; i < q; i++) {
            for (int j = 0; j < q; j++) {
                if (i == 0 && j == 0) continue;
                
                int src_rank;
                int src_coords[2] = {i, j};
                MPI_Cart_rank(grid_comm, src_coords, &src_rank);
                
                double* temp_block = malloc(block_elements * sizeof(double));
                MPI_Recv(temp_block, block_elements, MPI_DOUBLE, src_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // размещаем полученный блок в результирующей матрице
                for (int x = 0; x < block_size; x++) {
                    for (int y = 0; y < block_size; y++) {
                        int dest_row = i * block_size + x;
                        int dest_col = j * block_size + y;
                        C[dest_row * N + dest_col] = temp_block[x * block_size + y];
                    }
                }
                free(temp_block);
            }
        }
    } else {
        // остальные процессы отправляют свои результаты главному процессу
        MPI_Send(local_C, block_elements, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
    }
    
    // вывод результатов на главном процессе
    if (rank == 0) {
        double time = MPI_Wtime() - start_time;
        printf("Time: %.4f seconds\n", time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (time * 1e6));
        
        free(A); free(B); free(C);
    }
    
    // освобождаем локальную память
    free(local_A); free(local_B); free(local_C);
    MPI_Comm_free(&grid_comm);
    
    MPI_Finalize();
    return 0;
}
