#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Режимы передачи
#define MODE_STANDARD 0
#define MODE_SYNCHRONOUS 1
#define MODE_READY 2
#define MODE_BUFFERED 3

const char* mode_name(int mode) {
    switch(mode) {
        case MODE_STANDARD: return "STANDARD";
        case MODE_SYNCHRONOUS: return "SYNCHRONOUS";
        case MODE_READY: return "READY";
        case MODE_BUFFERED: return "BUFFERED";
        default: return "UNKNOWN";
    }
}

int main(int argc, char** argv) {
    int rank, size;
    int N = 256;
    int mode = MODE_STANDARD;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    if (argc > 2) mode = atoi(argv[2]);
    
    // Для буферизованного режима - ВСЕ процессы должны иметь буфер
    char* buffer = NULL;
    int buffer_size = 0;
    if (mode == MODE_BUFFERED) {
        buffer_size = N * N * sizeof(double) * 3; // буфер для всех передач
        buffer = malloc(buffer_size);
        MPI_Buffer_attach(buffer, buffer_size);
    }
    
    double start_time = 0.0;
    double* A = NULL;
    double* B = NULL;
    double* C = NULL;
    double* local_A = NULL;
    double* local_C = NULL;
    
    if (rank == 0) {
        printf("Matrix Multiplication - Mode: %s, Size: %dx%d, Processes: %d\n", 
               mode_name(mode), N, N, size);
        
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
    local_C = calloc(local_size, sizeof(double)); // обнуляем
    
    // Для READY режима - синхронизация
    if (mode == MODE_READY) {
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // Распределение матрицы A
    if (rank == 0) {
        // Свою часть копируем
        for (int i = 0; i < local_size; i++) {
            local_A[i] = A[i];
        }
        
        // Отправляем другим процессам
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            
            switch(mode) {
                case MODE_STANDARD:
                    MPI_Send(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_SYNCHRONOUS:
                    MPI_Ssend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_READY:
                    MPI_Rsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_BUFFERED:
                    MPI_Bsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
            }
        }
    } else {
        MPI_Recv(local_A, local_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // Распределение матрицы B
    if (rank != 0) {
        B = malloc(N * N * sizeof(double));
    }
    
    // Для READY режима - снова синхронизация перед отправкой B
    if (mode == MODE_READY) {
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            switch(mode) {
                case MODE_STANDARD:
                    MPI_Send(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
                    break;
                case MODE_SYNCHRONOUS:
                    MPI_Ssend(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
                    break;
                case MODE_READY:
                    MPI_Rsend(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
                    break;
                case MODE_BUFFERED:
                    MPI_Bsend(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
                    break;
            }
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
        FILE* fp = fopen("modes_complete_results.csv", "a");
        if (fp != NULL) {
            if (ftell(fp) == 0) {
                fprintf(fp, "size,processes,mode,time\n");
            }
            fprintf(fp, "%d,%d,%s,%.6f\n", N, size, mode_name(mode), execution_time);
            fclose(fp);
        }
        
        free(A);
        free(B);
        free(C);
    } else {
        // Отправляем результаты главному процессу
        switch(mode) {
            case MODE_STANDARD:
                MPI_Send(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
                break;
            case MODE_SYNCHRONOUS:
                MPI_Ssend(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
                break;
            case MODE_READY:
                MPI_Rsend(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
                break;
            case MODE_BUFFERED:
                MPI_Bsend(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
                break;
        }
        if (B != NULL) free(B);
    }
    
    // Освобождаем буфер для буферизованного режима
    if (mode == MODE_BUFFERED && buffer != NULL) {
        MPI_Buffer_detach(&buffer, &buffer_size);
        free(buffer);
    }
    
    free(local_A);
    free(local_C);
    
    MPI_Finalize();
    return 0;
}
