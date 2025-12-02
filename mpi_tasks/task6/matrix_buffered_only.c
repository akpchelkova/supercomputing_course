#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size;
    int N = 256;  // размер матрицы по умолчанию
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) N = atoi(argv[1]);
    
    // подготовка буфера ДО любых операций - важно для буферизованного режима
    int buffer_size = N * N * sizeof(double) * 4; // выделяем большой буфер
    char* buffer = malloc(buffer_size);
    // присоединяем буфер к mpi - это должно быть сделано на всех процессах
    MPI_Buffer_attach(buffer, buffer_size);
    
    double start_time = 0.0;
    double* A = NULL;
    double* B = NULL;
    double* C = NULL;
    double* local_A = NULL;
    double* local_C = NULL;
    
    // главный процесс инициализирует матрицы
    if (rank == 0) {
        printf("Matrix Multiplication - BUFFERED Mode, Size: %dx%d, Processes: %d\n", N, N, size);
        
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double)); 
        C = malloc(N * N * sizeof(double));
        
        // заполняем матрицы случайными значениями
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        start_time = MPI_Wtime();
    }
    
    // рассылаем размер матрицы всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // вычисляем количество строк на процесс
    int rows_per_proc = N / size;
    int local_size = rows_per_proc * N;
    
    // выделяем память под локальные части
    local_A = malloc(local_size * sizeof(double));
    local_C = calloc(local_size, sizeof(double)); // обнуляем результат
    
    // распределение матрицы A с использованием буферизованной отправки
    if (rank == 0) {
        // свою часть копируем напрямую
        for (int i = 0; i < local_size; i++) {
            local_A[i] = A[i];
        }
        
        // отправляем части другим процессам через Bsend
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            // буферизованная отправка - неблокирующая
            MPI_Bsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    } else {
        // процессы-получатели принимают свою часть
        MPI_Recv(local_A, local_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // распределение матрицы B (полная копия на все процессы)
    if (rank != 0) {
        B = malloc(N * N * sizeof(double));
    }
    
    if (rank == 0) {
        // отправляем матрицу B всем процессам через Bsend
        for (int i = 1; i < size; i++) {
            MPI_Bsend(B, N * N, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
        }
    } else {
        // процессы-получатели принимают матрицу B
        MPI_Recv(B, N * N, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // локальное умножение матриц
    for (int i = 0; i < rows_per_proc; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                sum += local_A[i * N + k] * B[k * N + j];
            }
            local_C[i * N + j] = sum;
        }
    }
    
    // сбор результатов на главном процессе
    if (rank == 0) {
        // копируем свою часть результата
        for (int i = 0; i < local_size; i++) {
            C[i] = local_C[i];
        }
        
        // получаем результаты от других процессов
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            MPI_Recv(&C[start_idx], local_size, MPI_DOUBLE, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        // вычисляем общее время выполнения
        double execution_time = MPI_Wtime() - start_time;
        printf("Execution time: %.6f seconds\n", execution_time);
        
        // сохраняем результаты в csv файл
        FILE* fp = fopen("buffered_results.csv", "a");
        if (fp != NULL) {
            fprintf(fp, "%d,%d,BUFFERED,%.6f\n", N, size, execution_time);
            fclose(fp);
        }
        
        // освобождаем память главного процесса
        free(A);
        free(B);
        free(C);
    } else {
        // отправляем результаты главному процессу через Bsend
        MPI_Bsend(local_C, local_size, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
        // освобождаем матрицу B на процессах-рабочих
        if (B != NULL) free(B);
    }
    
    // очистка буфера - отсоединяем и освобождаем память
    MPI_Buffer_detach(&buffer, &buffer_size);
    free(buffer);
    
    // освобождаем локальную память
    free(local_A);
    free(local_C);
    
    MPI_Finalize();
    return 0;
}
