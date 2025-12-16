#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv) {
    // инициализация mpi, создание коммуникатора mpi_comm_world
    MPI_Init(&argc, &argv);
    
    int world_rank, world_size;
    // получаем номер текущего процесса в mpi_comm_world
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    // получаем общее количество процессов в mpi_comm_world
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (argc < 2) {
        if (world_rank == 0) {
            fprintf(stderr, "использование: ./band n <число_узлов_опционально>\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    int n = atoi(argv[1]);  // размер матрицы
    
    // распределение строк матрицы между процессами
    int rows_per_proc = n / world_size;
    int remainder = n % world_size;
    
    int start_row, end_row, local_rows;
    
    // определяем какие строки будет обрабатывать каждый процесс
    if (world_rank < remainder) {
        start_row = world_rank * (rows_per_proc + 1);
        end_row = start_row + rows_per_proc;
    } else {
        start_row = world_rank * rows_per_proc + remainder;
        end_row = start_row + rows_per_proc - 1;
    }
    local_rows = end_row - start_row + 1;
    
    // выделение памяти для локальных блоков матриц
    double* a_block = (double*)malloc(local_rows * n * sizeof(double));
    double* c_block = (double*)calloc(local_rows * n, sizeof(double));
    double* b = (double*)malloc(n * n * sizeof(double));
    
    double* a_full = NULL;
    double* c_seq = NULL;
    
    // процесс 0 генерирует полные матрицы
    if (world_rank == 0) {
        a_full = (double*)malloc(n * n * sizeof(double));
        c_seq = (double*)calloc(n * n, sizeof(double));
        
        srand48(time(NULL));
        
        // генерация случайных матриц
        for (int i = 0; i < n * n; i++) {
            a_full[i] = drand48();
            b[i] = drand48();
        }
        
        // последовательное умножение для проверки результата
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                double sum = 0.0;
                for (int k = 0; k < n; k++) {
                    sum += a_full[i * n + k] * b[k * n + j];
                }
                c_seq[i * n + j] = sum;
            }
        }
        
        // распределение блоков матрицы a между процессами
        for (int p = 0; p < world_size; p++) {
            int p_start, p_end, p_rows;
            
            // вычисляем границы блока для процесса p
            if (p < remainder) {
                p_start = p * (rows_per_proc + 1);
                p_end = p_start + rows_per_proc;
            } else {
                p_start = p * rows_per_proc + remainder;
                p_end = p_start + rows_per_proc - 1;
            }
            p_rows = p_end - p_start + 1;
            
            if (p == 0) {
                // копируем данные для процесса 0
                for (int i = 0; i < p_rows; i++) {
                    for (int j = 0; j < n; j++) {
                        a_block[i * n + j] = a_full[(p_start + i) * n + j];
                    }
                }
            } else {
                // отправляем блок процессу p (точка-точка)
                MPI_Send(&a_full[p_start * n], p_rows * n, MPI_DOUBLE, p, 0, MPI_COMM_WORLD);
            }
        }
    } else {
        // остальные процессы получают свои блоки от процесса 0
        MPI_Recv(a_block, local_rows * n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // широковещательная рассылка матрицы b всем процессам
    // mpi_bcast передает данные от корневого процесса всем процессам в коммуникаторе
    MPI_Bcast(b, n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // синхронизация всех процессов перед началом вычислений
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();  // начало замера времени
    
    // параллельное умножение матриц (каждый процесс умножает свой блок)
    for (int i = 0; i < local_rows; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += a_block[i * n + k] * b[k * n + j];
            }
            c_block[i * n + j] = sum;
        }
    }
    
    double t_end = MPI_Wtime();  // конец замера времени
    double local_time = t_end - t_start;
    double max_time;
    
    // редукция для нахождения максимального времени среди всех процессов
    // mpi_reduce собирает данные со всех процессов и выполняет операцию (max)
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    // подготовка данных для сборки результатов на процессе 0
    int* recvcounts = NULL;
    int* displs = NULL;
    
    if (world_rank == 0) {
        recvcounts = (int*)malloc(world_size * sizeof(int));
        displs = (int*)malloc(world_size * sizeof(int));
        
        for (int p = 0; p < world_size; p++) {
            int p_start, p_end, p_rows;
            
            if (p < remainder) {
                p_start = p * (rows_per_proc + 1);
                p_end = p_start + rows_per_proc;
            } else {
                p_start = p * rows_per_proc + remainder;
                p_end = p_start + rows_per_proc - 1;
            }
            p_rows = p_end - p_start + 1;
            recvcounts[p] = p_rows * n;
            displs[p] = p_start * n;
        }
    }
    
    double* c_parallel = NULL;
    if (world_rank == 0) {
        c_parallel = (double*)malloc(n * n * sizeof(double));
    }
    
    // сборка результатов со всех процессов на процесс 0
    // mpi_gatherv собирает данные разного размера от всех процессов
    MPI_Gatherv(c_block, local_rows * n, MPI_DOUBLE,
                c_parallel, recvcounts, displs,
                MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // проверка корректности результата
    if (world_rank == 0) {
        double eps = 1e-12;
        int equal = 1;
        
        for (int i = 0; i < n * n; i++) {
            if (fabs(c_seq[i] - c_parallel[i]) > eps) {
                equal = 0;
                break;
            }
        }
        
        const char* nodes_arg = (argc > 2) ? argv[2] : "0";
        printf("%d,%d,%s,%.6f,check_equal=%s\n", 
               n, world_size, nodes_arg, max_time,
               equal ? "YES" : "NO");
               
        // освобождение памяти
        free(a_full);
        free(c_seq);
        free(c_parallel);
        free(recvcounts);
        free(displs);
    }
    
    // освобождение памяти
    free(a_block);
    free(c_block);
    free(b);
    
    // завершение работы с mpi
    MPI_Finalize();
    return 0;
}
