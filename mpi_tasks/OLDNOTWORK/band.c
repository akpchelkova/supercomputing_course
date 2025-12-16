#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size, N = 512;  // Увеличил стандартный размер
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    
    double start_time = 0;
    double *A = NULL, *B = NULL, *C = NULL;
    double *local_A, *local_C;
    
    // Для одного процесса используем простое умножение
    if (size == 1) {
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double));
        C = malloc(N * N * sizeof(double));
        
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        printf("Band Matrix Multiplication: %dx%d, %d processes\n", N, N, size);
        start_time = MPI_Wtime();
        
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                double sum = 0.0;
                for (int k = 0; k < N; k++) {
                    sum += A[i * N + k] * B[k * N + j];
                }
                C[i * N + j] = sum;
            }
        }
        
        double time = MPI_Wtime() - start_time;
        printf("Time: %.4f seconds\n", time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (time * 1e6));
        
        free(A); free(B); free(C);
        MPI_Finalize();
        return 0;
    }
    
    // Главный процесс создает матрицы
    if (rank == 0) {
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double));
        C = malloc(N * N * sizeof(double));
        
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        printf("Band Matrix Multiplication: %dx%d, %d processes\n", N, N, size);
        start_time = MPI_Wtime();
    }
    
    // Рассылаем размер
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Вычисляем локальный размер
    int local_rows = N / size;
    local_A = malloc(local_rows * N * sizeof(double));
    local_C = calloc(local_rows * N, sizeof(double)); // Используем calloc для инициализации нулями
    
    // Распределяем A по строкам
    MPI_Scatter(A, local_rows * N, MPI_DOUBLE, 
                local_A, local_rows * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // Рассылаем B всем
    if (rank != 0) B = malloc(N * N * sizeof(double));
    MPI_Bcast(B, N * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // Локальное умножение (оптимизированный порядок циклов)
    for (int i = 0; i < local_rows; i++) {
        for (int k = 0; k < N; k++) {
            double aik = local_A[i * N + k];
            for (int j = 0; j < N; j++) {
                local_C[i * N + j] += aik * B[k * N + j];
            }
        }
    }
    
    // Сбор результатов
    MPI_Gather(local_C, local_rows * N, MPI_DOUBLE,
               C, local_rows * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // Вывод времени
    if (rank == 0) {
        double time = MPI_Wtime() - start_time;
        printf("Time: %.4f seconds\n", time);
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (time * 1e6));
        
        free(A); free(B); free(C);
    }
    
    free(local_A); free(local_C);
    if (rank != 0) free(B);
    
    MPI_Finalize();
    return 0;
}
