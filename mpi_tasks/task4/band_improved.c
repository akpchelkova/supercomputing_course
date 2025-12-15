#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size, N;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    else N = 512;
    
    if (N > 4096) N = 4096;
    
    if (rank == 0) {
        printf("Band Algorithm (Improved)\n");
        printf("Matrix: %dx%d, Processes: %d\n", N, N, size);
    }
    
    double start_time = MPI_Wtime();
    
    // Проверяем делимость
    if (N % size != 0 && rank == 0) {
        printf("WARNING: N=%d not divisible by %d, using %d rows per process\n", 
               N, size, N/size);
    }
    
    int rows_per_proc = N / size;
    int extra_rows = N % size;
    
    // Определяем сколько строк обрабатывает каждый процесс
    int my_rows = rows_per_proc;
    if (rank < extra_rows) my_rows++;
    
    // Смещение для данного процесса
    int row_offset = rank * rows_per_proc;
    if (rank < extra_rows) row_offset += rank;
    else row_offset += extra_rows;
    
    // Выделяем память
    double *local_A = NULL;
    double *local_C = NULL;
    double *B = NULL;
    double *A = NULL;
    double *C = NULL;
    
    if (rank == 0) {
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double));
        C = malloc(N * N * sizeof(double));
        
        if (!A || !B || !C) {
            printf("Memory allocation failed!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // Инициализация
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
    } else {
        B = malloc(N * N * sizeof(double));
        if (!B) {
            printf("Process %d: Memory allocation failed!\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    
    // Рассылаем размер матрицы
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&my_rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&row_offset, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Выделяем память для локальных данных
    local_A = malloc(my_rows * N * sizeof(double));
    local_C = calloc(my_rows * N, sizeof(double));
    
    if (!local_A || !local_C) {
        printf("Process %d: Local memory allocation failed!\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Рассылаем матрицу B всем процессам
    MPI_Bcast(B, N * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // Распределяем строки матрицы A
    if (rank == 0) {
        // Копируем свои строки
        for (int i = 0; i < my_rows; i++) {
            for (int j = 0; j < N; j++) {
                local_A[i * N + j] = A[i * N + j];
            }
        }
        
        // Отправляем остальным процессам
        for (int p = 1; p < size; p++) {
            int p_rows = rows_per_proc;
            if (p < extra_rows) p_rows++;
            
            int p_offset = p * rows_per_proc;
            if (p < extra_rows) p_offset += p;
            else p_offset += extra_rows;
            
            for (int i = 0; i < p_rows; i++) {
                MPI_Send(&A[(p_offset + i) * N], N, MPI_DOUBLE, p, 0, MPI_COMM_WORLD);
            }
        }
    } else {
        // Получаем свои строки
        for (int i = 0; i < my_rows; i++) {
            MPI_Recv(&local_A[i * N], N, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    // Локальное умножение (оптимизированный порядок)
    for (int i = 0; i < my_rows; i++) {
        for (int k = 0; k < N; k++) {
            double aik = local_A[i * N + k];
            for (int j = 0; j < N; j++) {
                local_C[i * N + j] += aik * B[k * N + j];
            }
        }
    }
    
    // Сбор результатов
    if (rank == 0) {
        // Копируем свои результаты
        for (int i = 0; i < my_rows; i++) {
            for (int j = 0; j < N; j++) {
                C[(row_offset + i) * N + j] = local_C[i * N + j];
            }
        }
        
        // Получаем результаты от других процессов
        for (int p = 1; p < size; p++) {
            int p_rows = rows_per_proc;
            if (p < extra_rows) p_rows++;
            
            int p_offset = p * rows_per_proc;
            if (p < extra_rows) p_offset += p;
            else p_offset += extra_rows;
            
            double *temp_buf = malloc(p_rows * N * sizeof(double));
            MPI_Recv(temp_buf, p_rows * N, MPI_DOUBLE, p, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            for (int i = 0; i < p_rows; i++) {
                for (int j = 0; j < N; j++) {
                    C[(p_offset + i) * N + j] = temp_buf[i * N + j];
                }
            }
            free(temp_buf);
        }
    } else {
        // Отправляем свои результаты главному процессу
        MPI_Send(local_C, my_rows * N, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
    }
    
    double time = MPI_Wtime() - start_time;
    double max_time;
    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        // Проверяем результат
        double checksum = 0.0;
        for (int i = 0; i < N * N; i++) {
            checksum += C[i];
        }
        
        printf("Time: %.4f seconds\n", max_time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (max_time * 1e6));
        printf("Result checksum: %.2f\n", checksum);
        
        free(A); free(B); free(C);
    }
    
    free(local_A); free(local_C);
    if (rank != 0) free(B);
    
    MPI_Finalize();
    return 0;
}
