#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (world_rank == 0) {
        printf("=== Cartesian Grid Communication ===\n");
        printf("Total processes: %d\n", world_size);
    }
    
    // Создаем декартову топологию (решетку)
    int dims[2] = {0, 0};
    MPI_Dims_create(world_size, 2, dims);
    int rows = dims[0], cols = dims[1];
    
    if (world_rank == 0) {
        printf("Grid dimensions: %d x %d\n", rows, cols);
    }
    
    // Создаем декартову топологию
    int periods[2] = {0, 0}; // непериодическая решетка
    int reorder = 1;
    
    MPI_Comm grid_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, &grid_comm);
    
    int grid_rank, grid_coords[2];
    MPI_Comm_rank(grid_comm, &grid_rank);
    MPI_Cart_coords(grid_comm, grid_rank, 2, grid_coords);
    
    // Выводим информацию о распределении процессов
    printf("Process %d (world %d) -> Grid coordinates: (%d, %d)\n",
           grid_rank, world_rank, grid_coords[0], grid_coords[1]);
    
    // Создаем коммуникаторы для строк
    MPI_Comm row_comm;
    int row_color = grid_coords[0];
    int row_key = grid_coords[1];
    MPI_Comm_split(grid_comm, row_color, row_key, &row_comm);
    
    int row_rank, row_size;
    MPI_Comm_rank(row_comm, &row_rank);
    MPI_Comm_size(row_comm, &row_size);
    
    // Создаем коммуникаторы для столбцов
    MPI_Comm col_comm;
    int col_color = grid_coords[1];
    int col_key = grid_coords[0];
    MPI_Comm_split(grid_comm, col_color, col_key, &col_comm);
    
    int col_rank, col_size;
    MPI_Comm_rank(col_comm, &col_rank);
    MPI_Comm_size(col_comm, &col_size);
    
    printf("Process (%d,%d): row_rank=%d/%d, col_rank=%d/%d\n",
           grid_coords[0], grid_coords[1], row_rank, row_size, col_rank, col_size);
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Тестируем коллективные операции на разных коммуникаторах
    const int DATA_SIZE = 1000;
    const int NUM_ITERATIONS = 100;
    
    double* data = malloc(DATA_SIZE * sizeof(double));
    double* result = malloc(DATA_SIZE * sizeof(double));
    
    // Инициализация данных
    srand(time(NULL) + world_rank);
    for (int i = 0; i < DATA_SIZE; i++) {
        data[i] = (double)rand() / RAND_MAX * 100.0 + world_rank;
    }
    
    if (world_rank == 0) {
        printf("\n=== Testing Collective Operations ===\n");
        printf("Data size: %d doubles, Iterations: %d\n", DATA_SIZE, NUM_ITERATIONS);
    }
    
    // Тест 1: Broadcast на всем коммуникаторе WORLD
    MPI_Barrier(MPI_COMM_WORLD);
    double world_start = MPI_Wtime();
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        MPI_Bcast(data, DATA_SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    
    double world_end = MPI_Wtime();
    double world_time = world_end - world_start;
    
    // Тест 2: Broadcast на коммуникаторе строки
    MPI_Barrier(MPI_COMM_WORLD);
    double row_start = MPI_Wtime();
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // В каждой строке процесс с row_rank == 0 является корнем
        if (row_rank == 0) {
            MPI_Bcast(data, DATA_SIZE, MPI_DOUBLE, 0, row_comm);
        } else {
            MPI_Bcast(result, DATA_SIZE, MPI_DOUBLE, 0, row_comm);
        }
    }
    
    double row_end = MPI_Wtime();
    double row_time = row_end - row_start;
    
    // Тест 3: Reduce на всем коммуникаторе WORLD
    MPI_Barrier(MPI_COMM_WORLD);
    double world_reduce_start = MPI_Wtime();
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        MPI_Reduce(data, result, DATA_SIZE, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    
    double world_reduce_end = MPI_Wtime();
    double world_reduce_time = world_reduce_end - world_reduce_start;
    
    // Тест 4: Reduce на коммуникаторе столбца
    MPI_Barrier(MPI_COMM_WORLD);
    double col_reduce_start = MPI_Wtime();
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // В каждом столбце процесс с col_rank == 0 является корнем
        MPI_Reduce(data, result, DATA_SIZE, MPI_DOUBLE, MPI_SUM, 0, col_comm);
    }
    
    double col_reduce_end = MPI_Wtime();
    double col_reduce_time = col_reduce_end - col_reduce_start;
    
    // Сбор результатов на процессе 0
    if (world_rank == 0) {
        printf("\n=== Performance Results ===\n");
        printf("WORLD Bcast:    %.6f seconds\n", world_time);
        printf("ROW Bcast:      %.6f seconds\n", row_time);
        printf("Ratio (WORLD/ROW): %.2f\n", world_time / row_time);
        
        printf("\nWORLD Reduce:   %.6f seconds\n", world_reduce_time);
        printf("COL Reduce:     %.6f seconds\n", col_reduce_time);
        printf("Ratio (WORLD/COL): %.2f\n", world_reduce_time / col_reduce_time);
        
        // Сохраняем результаты в файл
        FILE* fp = fopen("cartesian_results.csv", "a");
        if (fp != NULL) {
            if (ftell(fp) == 0) {
                fprintf(fp, "processes,grid_size,world_bcast,row_bcast,world_reduce,col_reduce\n");
            }
            fprintf(fp, "%d,%dx%d,%.6f,%.6f,%.6f,%.6f\n",
                   world_size, rows, cols, world_time, row_time, world_reduce_time, col_reduce_time);
            fclose(fp);
        }
        
        printf("\n=== Summary ===\n");
        printf("Sub-communicators (rows/columns) are %.1f-%.1f times faster than WORLD communicator\n",
               fmin(world_time/row_time, world_reduce_time/col_reduce_time),
               fmax(world_time/row_time, world_reduce_time/col_reduce_time));
    }
    
    // Освобождаем коммуникаторы
    MPI_Comm_free(&grid_comm);
    MPI_Comm_free(&row_comm);
    MPI_Comm_free(&col_comm);
    
    free(data);
    free(result);
    
    if (world_rank == 0) {
        printf("\nCartesian grid tests completed successfully!\n");
    }
    
    MPI_Finalize();
    return 0;
}
