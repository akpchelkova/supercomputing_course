#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

int main(int argc, char** argv) {
    int rank, size, N;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Размер по умолчанию
    if (argc > 1) N = atoi(argv[1]);
    else N = 512;
    
    // Для безопасности ограничим максимальный размер
    if (N > 4096) N = 4096;
    
    if (rank == 0) {
        printf("Cannon Simple Algorithm\n");
        printf("Matrix: %dx%d, Processes: %d\n", N, N, size);
    }
    
    // Проверяем квадратное число процессов
    int q = (int)sqrt(size);
    if (q * q != size) {
        if (rank == 0) {
            printf("ERROR: Number of processes (%d) must be perfect square (1,4,9,16...)\n", size);
        }
        MPI_Finalize();
        return 1;
    }
    
    // Проверяем делимость размера
    if (N % q != 0) {
        if (rank == 0) {
            printf("ERROR: Matrix size %d must be divisible by %d\n", N, q);
        }
        MPI_Finalize();
        return 1;
    }
    
    int block_size = N / q;
    int block_elems = block_size * block_size;
    
    if (rank == 0) {
        printf("Grid: %dx%d, Block size: %dx%d\n", q, q, block_size, block_size);
    }
    
    double start_time = MPI_Wtime();
    
    // Выделяем память для локальных блоков
    double *local_A = malloc(block_elems * sizeof(double));
    double *local_B = malloc(block_elems * sizeof(double));
    double *local_C = calloc(block_elems, sizeof(double));
    
    if (!local_A || !local_B || !local_C) {
        printf("Process %d: Memory allocation failed!\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Инициализируем тестовыми данными
    for (int i = 0; i < block_elems; i++) {
        local_A[i] = (double)(rank + 1);  // Разные значения для разных процессов
        local_B[i] = 1.0;
    }
    
    // Простая версия алгоритма Кэннона
    // Создаем декартову топологию
    MPI_Comm grid_comm;
    int dims[2] = {q, q};
    int periods[2] = {1, 1};  // Циклическая
    int reorder = 0;
    
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, &grid_comm);
    
    // Получаем координаты в сетке
    int coords[2];
    MPI_Cart_coords(grid_comm, rank, 2, coords);
    int row = coords[0];
    int col = coords[1];
    
    // Определяем соседей для циклических сдвигов
    int left_rank, right_rank, up_rank, down_rank;
    MPI_Cart_shift(grid_comm, 1, -row, &left_rank, &right_rank);  // Сдвиг A влево
    MPI_Cart_shift(grid_comm, 0, -col, &up_rank, &down_rank);     // Сдвиг B вверх
    
    // Основной цикл алгоритма Кэннона
    for (int step = 0; step < q; step++) {
        // Локальное умножение
        for (int i = 0; i < block_size; i++) {
            for (int k = 0; k < block_size; k++) {
                double aik = local_A[i * block_size + k];
                for (int j = 0; j < block_size; j++) {
                    local_C[i * block_size + j] += aik * local_B[k * block_size + j];
                }
            }
        }
        
        // Циркуляция блоков (кроме последней итерации)
        if (step < q - 1) {
            // Сдвиг A влево
            MPI_Sendrecv_replace(local_A, block_elems, MPI_DOUBLE,
                                left_rank, 0, right_rank, 0,
                                grid_comm, MPI_STATUS_IGNORE);
            
            // Сдвиг B вверх
            MPI_Sendrecv_replace(local_B, block_elems, MPI_DOUBLE,
                                up_rank, 0, down_rank, 0,
                                grid_comm, MPI_STATUS_IGNORE);
        }
    }
    
    // Проверяем результат (сумма всех элементов)
    double local_sum = 0.0;
    for (int i = 0; i < block_elems; i++) {
        local_sum += local_C[i];
    }
    
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, grid_comm);
    
    double time = MPI_Wtime() - start_time;
    
    // Собираем время со всех процессов
    double max_time;
    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Time: %.4f seconds\n", max_time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (max_time * 1e6));
        printf("Result checksum: %.2f\n", global_sum);
        printf("Expected checksum: %.2f\n", (double)q * (q + 1) / 2.0 * N * block_size);
    }
    
    // Освобождаем ресурсы
    free(local_A);
    free(local_B);
    free(local_C);
    MPI_Comm_free(&grid_comm);
    
    MPI_Finalize();
    return 0;
}
