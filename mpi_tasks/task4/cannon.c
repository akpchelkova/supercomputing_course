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
    
    // Проверяем квадратное число процессов
    int q = (int)sqrt(size);
    if (q * q != size) {
        if (rank == 0) {
            printf("Error: Number of processes must be a perfect square (1, 4, 9, 16...)\n");
            printf("Current: %d\n", size);
        }
        MPI_Finalize();
        return 1;
    }
    
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
    
    // Главный процесс создает матрицы
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
    
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&block_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Локальные блоки
    int block_elements = block_size * block_size;
    local_A = malloc(block_elements * sizeof(double));
    local_B = malloc(block_elements * sizeof(double));
    local_C = calloc(block_elements, sizeof(double));
    
    // Создаем декартову топологию
    MPI_Comm grid_comm;
    int dims[2] = {q, q};
    int periods[2] = {1, 1}; // циклическая
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &grid_comm);
    
    int coords[2];
    MPI_Cart_coords(grid_comm, rank, 2, coords);
    int my_i = coords[0], my_j = coords[1];
    
    // Распределяем данные - каждый процесс получает свой блок
    if (rank == 0) {
        // Главный процесс отправляет блоки другим процессам
        for (int i = 0; i < q; i++) {
            for (int j = 0; j < q; j++) {
                if (i == 0 && j == 0) continue; // себе не отправляем
                
                int dest_rank;
                int dest_coords[2] = {i, j};
                MPI_Cart_rank(grid_comm, dest_coords, &dest_rank);
                
                // Отправляем блок A[i,j]
                for (int x = 0; x < block_size; x++) {
                    int src_row = i * block_size + x;
                    MPI_Send(&A[src_row * N + j * block_size], block_size, MPI_DOUBLE, 
                            dest_rank, 0, MPI_COMM_WORLD);
                }
                
                // Отправляем блок B[i,j]  
                for (int x = 0; x < block_size; x++) {
                    int src_row = i * block_size + x;
                    MPI_Send(&B[src_row * N + j * block_size], block_size, MPI_DOUBLE,
                            dest_rank, 1, MPI_COMM_WORLD);
                }
            }
        }
        
        // Главный процесс копирует свои блоки
        for (int x = 0; x < block_size; x++) {
            int src_row = x;
            for (int y = 0; y < block_size; y++) {
                local_A[x * block_size + y] = A[src_row * N + y];
                local_B[x * block_size + y] = B[src_row * N + y];
            }
        }
    } else {
        // Остальные процессы получают свои блоки
        for (int x = 0; x < block_size; x++) {
            MPI_Recv(&local_A[x * block_size], block_size, MPI_DOUBLE, 
                    0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&local_B[x * block_size], block_size, MPI_DOUBLE,
                    0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    // Начальная перестановка A (сдвиг влево на i позиций)
    int left_rank, right_rank;
    MPI_Cart_shift(grid_comm, 1, -my_i, &left_rank, &right_rank);
    MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                        left_rank, 0, right_rank, 0, grid_comm, MPI_STATUS_IGNORE);
    
    // Начальная перестановка B (сдвиг вверх на j позиций)  
    int up_rank, down_rank;
    MPI_Cart_shift(grid_comm, 0, -my_j, &up_rank, &down_rank);
    MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                        up_rank, 0, down_rank, 0, grid_comm, MPI_STATUS_IGNORE);
    
    // Основной цикл алгоритма Кэннона
    for (int step = 0; step < q; step++) {
        // Локальное умножение матриц
        for (int i = 0; i < block_size; i++) {
            for (int j = 0; j < block_size; j++) {
                for (int k = 0; k < block_size; k++) {
                    local_C[i * block_size + j] += 
                        local_A[i * block_size + k] * local_B[k * block_size + j];
                }
            }
        }
        
        // Циркуляция блоков (кроме последней итерации)
        if (step < q - 1) {
            MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                                left_rank, 0, right_rank, 0, grid_comm, MPI_STATUS_IGNORE);
            MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                                up_rank, 0, down_rank, 0, grid_comm, MPI_STATUS_IGNORE);
        }
    }
    
    // Сбор результатов
    if (rank == 0) {
        // Главный процесс собирает результаты
        for (int x = 0; x < block_size; x++) {
            for (int y = 0; y < block_size; y++) {
                C[x * N + y] = local_C[x * block_size + y];
            }
        }
        
        for (int i = 0; i < q; i++) {
            for (int j = 0; j < q; j++) {
                if (i == 0 && j == 0) continue;
                
                int src_rank;
                int src_coords[2] = {i, j};
                MPI_Cart_rank(grid_comm, src_coords, &src_rank);
                
                double* temp_block = malloc(block_elements * sizeof(double));
                MPI_Recv(temp_block, block_elements, MPI_DOUBLE, src_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
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
        // Остальные процессы отправляют свои результаты
        MPI_Send(local_C, block_elements, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
    }
    
    // Вывод времени
    if (rank == 0) {
        double time = MPI_Wtime() - start_time;
        printf("Time: %.4f seconds\n", time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (time * 1e6));
        
        free(A); free(B); free(C);
    }
    
    free(local_A); free(local_B); free(local_C);
    MPI_Comm_free(&grid_comm);
    
    MPI_Finalize();
    return 0;
}
