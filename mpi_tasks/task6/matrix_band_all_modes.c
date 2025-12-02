#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// определяем константы для разных режимов передачи
#define MODE_STANDARD 0
#define MODE_SYNCHRONOUS 1
#define MODE_READY 2
#define MODE_BUFFERED 3

// функция для получения имени режима по его коду
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
    int N = 256;  // размер матрицы по умолчанию
    int mode = MODE_STANDARD;  // режим передачи по умолчанию
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // читаем параметры из командной строки
    if (argc > 1) N = atoi(argv[1]);
    if (argc > 2) mode = atoi(argv[2]);
    
    // для буферизованного режима - все процессы должны иметь буфер
    char* buffer = NULL;
    int buffer_size = 0;
    if (mode == MODE_BUFFERED) {
        // выделяем достаточно большой буфер для всех передач
        buffer_size = N * N * sizeof(double) * 3;
        buffer = malloc(buffer_size);
        // присоединяем буфер к mpi
        MPI_Buffer_attach(buffer, buffer_size);
    }
    
    double start_time = 0.0;
    double* A = NULL;
    double* B = NULL;
    double* C = NULL;
    double* local_A = NULL;
    double* local_C = NULL;
    
    // главный процесс инициализирует матрицы
    if (rank == 0) {
        printf("Matrix Multiplication - Mode: %s, Size: %dx%d, Processes: %d\n", 
               mode_name(mode), N, N, size);
        
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double)); 
        C = malloc(N * N * sizeof(double));
        
        // заполняем матрицы случайными значениями
        srand(time(NULL));
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        // начинаем замер времени
        start_time = MPI_Wtime();
    }
    
    // рассылаем размер матрицы всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // вычисляем количество строк на процесс
    int rows_per_proc = N / size;
    int local_size = rows_per_proc * N;
    
    // выделяем память под локальные части матриц
    local_A = malloc(local_size * sizeof(double));
    local_C = calloc(local_size, sizeof(double)); // обнуляем результирующую матрицу
    
    // для ready режима нужна синхронизация перед отправкой
    if (mode == MODE_READY) {
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // распределение матрицы A по процессам
    if (rank == 0) {
        // свою часть копируем напрямую
        for (int i = 0; i < local_size; i++) {
            local_A[i] = A[i];
        }
        
        // отправляем части другим процессам в зависимости от режима
        for (int i = 1; i < size; i++) {
            int start_idx = i * local_size;
            
            switch(mode) {
                case MODE_STANDARD:
                    // стандартная отправка - может быть буферизованной или синхронной
                    MPI_Send(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_SYNCHRONOUS:
                    // синхронная отправка - блокируется пока получатель не начнет прием
                    MPI_Ssend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_READY:
                    // готовностная отправка - предполагает что получатель уже ждет
                    MPI_Rsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
                case MODE_BUFFERED:
                    // буферизованная отправка - данные копируются в буфер
                    MPI_Bsend(&A[start_idx], local_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    break;
            }
        }
    } else {
        // процессы-получатели принимают свою часть матрицы A
        MPI_Recv(local_A, local_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // распределение матрицы B (полная копия на все процессы)
    if (rank != 0) {
        B = malloc(N * N * sizeof(double));
    }
    
    // для ready режима - снова синхронизация перед отправкой B
    if (mode == MODE_READY) {
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        // отправляем матрицу B всем процессам
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
        // процессы-получатели принимают матрицу B
        MPI_Recv(B, N * N, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // локальное умножение матриц - каждый процесс умножает свою часть A на всю B
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
        FILE* fp = fopen("modes_complete_results.csv", "a");
        if (fp != NULL) {
            // если файл пустой, записываем заголовок
            if (ftell(fp) == 0) {
                fprintf(fp, "size,processes,mode,time\n");
            }
            // записываем строку с результатами
            fprintf(fp, "%d,%d,%s,%.6f\n", N, size, mode_name(mode), execution_time);
            fclose(fp);
        }
        
        // освобождаем память главного процесса
        free(A);
        free(B);
        free(C);
    } else {
        // отправляем результаты главному процессу в том же режиме
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
        // освобождаем матрицу B на процессах-рабочих
        if (B != NULL) free(B);
    }
    
    // освобождаем буфер для буферизованного режима
    if (mode == MODE_BUFFERED && buffer != NULL) {
        MPI_Buffer_detach(&buffer, &buffer_size);
        free(buffer);
    }
    
    // освобождаем локальную память
    free(local_A);
    free(local_C);
    
    MPI_Finalize();
    return 0;
}
