#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>     
#include <time.h>
#include <limits.h>

// Генерация случайной части вектора
void generate_vector_part(double *vector, int size, int seed_offset) {
    unsigned int seed = time(NULL) + seed_offset * 12345;
    for (int i = 0; i < size; i++) {
        vector[i] = (double)rand_r(&seed) / RAND_MAX * 1000.0;
    }
}

void find_local_min_max(double *vector, int local_size, double *local_min, double *local_max) {
    *local_min = vector[0];
    *local_max = vector[0];
    
    for (int i = 1; i < local_size; i++) {
        if (vector[i] < *local_min) *local_min = vector[i];
        if (vector[i] > *local_max) *local_max = vector[i];
    }
}

void run_parallel_experiment(int vector_size, int use_processes, int world_rank, int world_size) {
    MPI_Comm comm;
    int color = (world_rank < use_processes) ? 0 : MPI_UNDEFINED;
    MPI_Comm_split(MPI_COMM_WORLD, color, world_rank, &comm);
    
    if (color == 0) {
        int rank, size;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
        
        // Вычисляем размер локальной части
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        // Синхронизируем и замеряем время
        MPI_Barrier(comm);
        double start_time = MPI_Wtime();
        
        // Каждый процесс генерирует свою часть
        double *local_vector = (double *)malloc(local_size * sizeof(double));
        if (local_vector == NULL) {
            fprintf(stderr, "Process %d: Failed to allocate %ld bytes for vector size %d\n", 
                    rank, (long)(local_size * sizeof(double)), local_size);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        generate_vector_part(local_vector, local_size, rank);
        
        // Находим локальные минимум/максимум
        double local_min, local_max;
        find_local_min_max(local_vector, local_size, &local_min, &local_max);
        
        // Собираем результаты
        double global_min, global_max;
        MPI_Reduce(&local_min, &global_min, 1, MPI_DOUBLE, MPI_MIN, 0, comm);
        MPI_Reduce(&local_max, &global_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        
        double end_time = MPI_Wtime();
        double elapsed_time = end_time - start_time;
        
        // Находим максимальное время среди всех процессов
        double max_time;
        MPI_Reduce(&elapsed_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        
        if (rank == 0) {
            printf("PARALLEL: processes=%2d, vector_size=%-12d, time=%9.6f sec\n", 
                   size, vector_size, max_time);
            // printf("  min=%.2f, max=%.2f\n", global_min, global_max);
        }
        
        free(local_vector);
        MPI_Comm_free(&comm);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
}

void run_sequential_experiment(int vector_size, int world_rank) {
    if (world_rank == 0) {
        printf("\n=== SEQUENTIAL VERSION (Baseline) ===\n");
        
        // Меньшие размеры для последовательной версии
        int seq_sizes[] = {100, 1000, 10000, 100000, 1000000, 5000000};
        int num_seq_tests = sizeof(seq_sizes) / sizeof(seq_sizes[0]);
        
        for (int j = 0; j < num_seq_tests; j++) {
            int size = seq_sizes[j];
            double *vector = (double *)malloc(size * sizeof(double));
            if (vector == NULL) {
                fprintf(stderr, "Failed to allocate memory for size %d\n", size);
                continue;
            }
            
            double start_time = MPI_Wtime();
            
            // Генерация
            srand(time(NULL));
            for (int i = 0; i < size; i++) {
                vector[i] = (double)rand() / RAND_MAX * 1000.0;
            }
            
            // Поиск min/max
            double min_val = vector[0];
            double max_val = vector[0];
            for (int i = 1; i < size; i++) {
                if (vector[i] < min_val) min_val = vector[i];
                if (vector[i] > max_val) max_val = vector[i];
            }
            
            double end_time = MPI_Wtime();
            
            printf("SEQUENTIAL: processes= 1, vector_size=%-12d, time=%9.6f sec\n", 
                   size, end_time - start_time);
            // printf("  min=%.2f, max=%.2f\n", min_val, max_val);
            
            free(vector);
        }
        printf("\n=== PARALLEL VERSION (MPI) ===\n");
    }
}

int main(int argc, char *argv[]) {
    int world_rank, world_size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (world_rank == 0) {
        printf("=============================================================\n");
        printf("MPI Min/Max Search Performance Test\n");
        printf("Total available processes: %d\n", world_size);
        printf("=============================================================\n");
    }
    
    // Широкий диапазон размеров векторов
    long vector_sizes[] = {
        100,           // Очень маленький
        1000,          // Маленький
        10000,         // Средний
        100000,        // Большой
        1000000,       // Очень большой
        5000000,       // 5 миллионов
        10000000,      // 10 миллионов
        25000000,      // 25 миллионов
        50000000,      // 50 миллионов
        100000000      // 100 миллионов (ОЧЕНЬ большой)
    };
    
    int processes_to_test[] = {1, 2, 4, 8, 16, 32};
    
    int num_size_tests = sizeof(vector_sizes) / sizeof(vector_sizes[0]);
    int num_proc_tests = sizeof(processes_to_test) / sizeof(processes_to_test[0]);
    
    // Запускаем последовательную версию только для процессора 0
    run_sequential_experiment(0, world_rank);
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Запускаем параллельные эксперименты
    for (int i = 0; i < num_proc_tests; i++) {
        int num_procs = processes_to_test[i];
        
        if (num_procs <= world_size) {
            if (world_rank == 0) {
                printf("\n--- Testing with %2d processes ---\n", num_procs);
            }
            
            for (int j = 0; j < num_size_tests; j++) {
                long vec_size = vector_sizes[j];
                
                // Пропускаем слишком большие размеры для малого числа процессов
                if (num_procs == 1 && vec_size > 10000000) continue;
                if (num_procs == 2 && vec_size > 50000000) continue;
                
                // Проверяем доступность памяти (примерно 8 байт на элемент)
                long estimated_mem = vec_size * 8L / num_procs;
                if (estimated_mem > 2000000000L) { // ~2GB на процесс
                    if (world_rank == 0) {
                        printf("Skipping size %ld with %d processes (memory limit)\n", 
                               vec_size, num_procs);
                    }
                    continue;
                }
                
                run_parallel_experiment(vec_size, num_procs, world_rank, world_size);
            }
        }
    }
    
    if (world_rank == 0) {
        printf("\n=============================================================\n");
        printf("All experiments completed successfully!\n");
        printf("=============================================================\n");
    }
    
    MPI_Finalize();
    return 0;
}
