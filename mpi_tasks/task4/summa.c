#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv) {
    // инициализация mpi
    MPI_Init(&argc, &argv);
    
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (argc < 2) {
        if (world_rank == 0) {
            fprintf(stderr, "использование: ./summa n <число_узлов_опционально>\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    int n = atoi(argv[1]);  // размер матрицы
    
    // создание декартовой топологии
    // определяем размеры сетки процессов (близко к квадрату)
    int grid_rows = (int)sqrt((double)world_size);
    while (world_size % grid_rows != 0) {
        grid_rows--;
    }
    int grid_cols = world_size / grid_rows;
    
    // проверка что размер делится на оба измерения сетки
    if (n % grid_rows != 0 || n % grid_cols != 0) {
        if (world_rank == 0) {
            fprintf(stderr, "ошибка: размер %d не делится на размеры сетки %dx%d\n", 
                    n, grid_rows, grid_cols);
        }
        MPI_Finalize();
        return 1;
    }
    
    // создание 2d сетки
    int dims[2] = {grid_rows, grid_cols};
    int periods[2] = {0, 0};  // непериодическая сетка для summa
    MPI_Comm grid_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &grid_comm);
    
    int grid_rank;
    MPI_Comm_rank(grid_comm, &grid_rank);
    
    // получение координат в сетке
    int coords[2];
    MPI_Cart_coords(grid_comm, grid_rank, 2, coords);
    int row = coords[0];
    int col = coords[1];
    
    // размер блока на процесс
    int block_rows = n / grid_rows;
    int block_cols = n / grid_cols;
    
    // выделение памяти для локальных блоков
    double* a_block = (double*)malloc(block_rows * block_cols * sizeof(double));
    double* b_block = (double*)malloc(block_rows * block_cols * sizeof(double));
    double* c_block = (double*)calloc(block_rows * block_cols, sizeof(double));
    
    double* a_full = NULL;
    double* b_full = NULL;
    double* c_seq = NULL;
    
    // процесс 0 генерирует полные матрицы
    if (world_rank == 0) {
        a_full = (double*)malloc(n * n * sizeof(double));
        b_full = (double*)malloc(n * n * sizeof(double));
        c_seq = (double*)calloc(n * n, sizeof(double));
        
        srand48(time(NULL));
        
        // генерация случайных матриц
        for (int i = 0; i < n * n; i++) {
            a_full[i] = drand48();
            b_full[i] = drand48();
        }
        
        // последовательное умножение для проверки
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                double sum = 0.0;
                for (int k = 0; k < n; k++) {
                    sum += a_full[i * n + k] * b_full[k * n + j];
                }
                c_seq[i * n + j] = sum;
            }
        }
    }
    
    // распределение блоков матрицы a по процессам (по строкам)
    // каждый процесс получает свой блок a_block[row][col]
    if (world_rank == 0) {
        // процесс 0 копирует свой блок
        for (int i = 0; i < block_rows; i++) {
            for (int j = 0; j < block_cols; j++) {
                a_block[i * block_cols + j] = 
                    a_full[i * n + j];
                b_block[i * block_cols + j] = 
                    b_full[i * n + j];
            }
        }
        
        // отправляем блоки остальным процессам
        for (int p = 1; p < world_size; p++) {
            // получаем координаты процесса p
            int p_coords[2];
            MPI_Cart_coords(grid_comm, p, 2, p_coords);
            int p_row = p_coords[0];
            int p_col = p_coords[1];
            
            // подготавливаем блоки для процесса p
            double* send_a = (double*)malloc(block_rows * block_cols * sizeof(double));
            double* send_b = (double*)malloc(block_rows * block_cols * sizeof(double));
            
            for (int i = 0; i < block_rows; i++) {
                for (int j = 0; j < block_cols; j++) {
                    // глобальные координаты элемента
                    int global_i = p_row * block_rows + i;
                    int global_j = p_col * block_cols + j;
                    
                    send_a[i * block_cols + j] = a_full[global_i * n + global_j];
                    send_b[i * block_cols + j] = b_full[global_i * n + global_j];
                }
            }
            
            // отправляем блоки
            MPI_Send(send_a, block_rows * block_cols, MPI_DOUBLE, p, 0, grid_comm);
            MPI_Send(send_b, block_rows * block_cols, MPI_DOUBLE, p, 1, grid_comm);
            
            free(send_a);
            free(send_b);
        }
    } else {
        // остальные процессы получают свои блоки
        MPI_Recv(a_block, block_rows * block_cols, MPI_DOUBLE, 0, 0, grid_comm, MPI_STATUS_IGNORE);
        MPI_Recv(b_block, block_rows * block_cols, MPI_DOUBLE, 0, 1, grid_comm, MPI_STATUS_IGNORE);
    }
    
    // создание коммуникаторов для строк и столбцов
    MPI_Comm row_comm, col_comm;
    MPI_Comm_split(grid_comm, row, col, &row_comm);  // все процессы с одинаковым row
    MPI_Comm_split(grid_comm, col, row, &col_comm);  // все процессы с одинаковым col
    
    // буферы для широковещательных рассылок
    double* a_bcast = (double*)malloc(block_rows * block_cols * sizeof(double));
    double* b_bcast = (double*)malloc(block_rows * block_cols * sizeof(double));
    
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();  // начало замера времени
    
    // алгоритм summa
    for (int k = 0; k < grid_cols; k++) {
        // процесс с координатами [row, k] рассылает свой блок a по всей строке
        if (col == k) {
            memcpy(a_bcast, a_block, block_rows * block_cols * sizeof(double));
        }
        MPI_Bcast(a_bcast, block_rows * block_cols, MPI_DOUBLE, k, row_comm);
        
        // процесс с координатами [k, col] рассылает свой блок b по всему столбцу
        if (row == k) {
            memcpy(b_bcast, b_block, block_rows * block_cols * sizeof(double));
        }
        MPI_Bcast(b_bcast, block_rows * block_cols, MPI_DOUBLE, k, col_comm);
        
        // локальное умножение и сложение
        for (int i = 0; i < block_rows; i++) {
            for (int j = 0; j < block_cols; j++) {
                double sum = 0.0;
                for (int t = 0; t < block_cols; t++) {
                    sum += a_bcast[i * block_cols + t] * b_bcast[t * block_cols + j];
                }
                c_block[i * block_cols + j] += sum;
            }
        }
    }
    
    double t_end = MPI_Wtime();  // конец замера времени
    double local_time = t_end - t_start;
    double max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    // сбор результатов на процессе 0
    double* c_parallel = NULL;
    if (world_rank == 0) {
        c_parallel = (double*)calloc(n * n, sizeof(double));
    }
    
    // каждый процесс отправляет свой блок c процессу 0
    for (int p = 0; p < world_size; p++) {
        if (world_rank == p) {
            if (world_rank == 0) {
                // процесс 0 копирует свой блок
                for (int i = 0; i < block_rows; i++) {
                    for (int j = 0; j < block_cols; j++) {
                        c_parallel[i * n + j] = c_block[i * block_cols + j];
                    }
                }
            } else {
                // остальные процессы отправляют свои блоки
                MPI_Send(c_block, block_rows * block_cols, MPI_DOUBLE, 0, 2, grid_comm);
            }
        } else if (world_rank == 0) {
            // процесс 0 получает блоки от других процессов
            double* tmp = (double*)malloc(block_rows * block_cols * sizeof(double));
            MPI_Recv(tmp, block_rows * block_cols, MPI_DOUBLE, p, 2, grid_comm, MPI_STATUS_IGNORE);
            
            // получаем координаты процесса p
            int p_coords[2];
            MPI_Cart_coords(grid_comm, p, 2, p_coords);
            int p_row = p_coords[0];
            int p_col = p_coords[1];
            
            // размещаем блок в правильной позиции
            for (int i = 0; i < block_rows; i++) {
                for (int j = 0; j < block_cols; j++) {
                    int global_i = p_row * block_rows + i;
                    int global_j = p_col * block_cols + j;
                    c_parallel[global_i * n + global_j] = tmp[i * block_cols + j];
                }
            }
            
            free(tmp);
        }
    }
    
    // проверка корректности результата
    if (world_rank == 0) {
        double eps = 1e-12;
        int equal = 1;
        
        for (int i = 0; i < n * n; i++) {
            if (fabs(c_seq[i] - c_parallel[i]) > eps) {
                equal = 0;
                printf("ошибка на элементе %d: seq=%f, parallel=%f, diff=%e\n", 
                       i, c_seq[i], c_parallel[i], fabs(c_seq[i] - c_parallel[i]));
                break;
            }
        }
        
        const char* nodes_arg = (argc > 2) ? argv[2] : "0";
        printf("%d,%d,%s,%.6f,check_equal=%s\n", 
               n, world_size, nodes_arg, max_time,
               equal ? "YES" : "NO");
               
        // освобождение памяти
        free(a_full);
        free(b_full);
        free(c_seq);
        free(c_parallel);
    }
    
    // освобождение памяти
    free(a_block);
    free(b_block);
    free(c_block);
    free(a_bcast);
    free(b_bcast);
    
    // освобождение коммуникаторов
    MPI_Comm_free(&row_comm);
    MPI_Comm_free(&col_comm);
    MPI_Comm_free(&grid_comm);
    
    // завершение mpi
    MPI_Finalize();
    return 0;
}
