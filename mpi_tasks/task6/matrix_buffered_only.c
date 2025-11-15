#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size;
    int N = 256;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    
    // Подготовка буфера ДО любых операций
    int buffer_size = N * N * sizeof(double) * 4; // большой буфер
    char* buffer = malloc(buffer_size);
    MPI_Buffer_attach(buffer, buffer_size);
    
    double start_time = 0.0;
    double* A = NULL;
    double* B = NULL;
    double* C = NULL;
    double* local_A = NULL;
    double* local_C = NULL;
    
    if (rank == 0) {
        printf("Matrix Multiplication - BUFFERED Mode, Size: %dx%d, Processes: %d\n", N, N, size);
        
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double)); 
        C = malloc(N * N * sizeof(double));
        
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        start_time = MPI_Wtime();
    }
    
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    int rows_per_proc = N / size;
    int local_size = rows_per_proc * N;
    
    local_A = malloc(local_size * sizeof(double));
    local_C = calloc(local_size, sizeof(double));
    
    // Распределение матрицы A
    if (rank == 0) {
        // Свою часть копируем
        for (int i = 0; i < local_size; i++) {
            local_A[i] = A[i];
        }
        
        // Отправляем другим процессам через Bsend
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            MPI_Bsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(local_A, local_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // Распределение матрицы B
    if (rank != 0) {
        B = malloc(N * N * sizeof(double));
    }
    
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            MPI_Bsend(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(B, N * N, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // Локальное умножение
    for (int i = 0; i < rows_per_proc; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                sum += local_A[i * N + k] * B[k * N + j];
            }
            local_C[i * N + j] = sum;
        }
    }
    
    // Сбор результатов
    if (rank == 0) {
        // Копируем свою часть
        for (int i = 0; i < local_size; i++) {
            C[i] = local_C[i];
        }
        
        // Получаем от других процессов
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            MPI_Recv(&C[start_idx], local_size, MPI_DOUBLE, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        double execution_time = MPI_Wtime() - start_time;
        printf("Execution time: %.6f seconds\n", execution_time);
        
        // Сохраняем результаты
        FILE* fp = fopen("buffered_results.csv", "a");
        if (fp != NULL) {
            fprintf(fp, "%d,%d,BUFFERED,%.6f\n", N, size, execution_time);
            fclose(fp);
        }
        
        free(A);
        free(B);
        free(C);
    } else {
        // Отправляем результаты через Bsend
        MPI_Bsend(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
        if (B != NULL) free(B);
    }
    
    // Очистка буфера
    MPI_Buffer_detach(&buffer, &buffer_size);
    free(buffer);
    
    free(local_A);
    free(local_C);
    
    MPI_Finalize();
    return 0;
}
