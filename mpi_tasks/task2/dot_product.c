#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>        
#include <time.h>
#include <math.h>

// Функция генерации случайной части вектора (каждый процесс генерирует свою часть)
void generate_vector_part(double *vector, int size, int seed_offset) {
    unsigned int seed = time(NULL) + seed_offset * 12345;
    for (int i = 0; i < size; i++) {
        vector[i] = (double)rand_r(&seed) / RAND_MAX * 10.0;
    }
}

// Функция вычисления локального скалярного произведения
double compute_local_dot_product(double *vec1, double *vec2, int local_size) {
    double local_dot = 0.0;
    // Используем цикл с автовекторизацией
    for (int i = 0; i < local_size; i++) {
        local_dot += vec1[i] * vec2[i];
    }
    return local_dot;
}

// Функция проведения эксперимента с правильным замером времени
void run_experiment(int vector_size, int use_processes, int world_rank, int world_size) {
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
        
        // Выделяем память под локальные части
        double *local_vec1 = (double *)malloc(local_size * sizeof(double));
        double *local_vec2 = (double *)malloc(local_size * sizeof(double));
        
        if (local_vec1 == NULL || local_vec2 == NULL) {
            fprintf(stderr, "Process %d: Memory allocation failed!\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // СИНХРОНИЗИРУЕМ ПЕРЕД НАЧАЛОМ ИЗМЕРЕНИЯ ВРЕМЕНИ
        MPI_Barrier(comm);
        double start_time = MPI_Wtime();
        
        // Каждый процесс генерирует СВОЮ часть независимо
        generate_vector_part(local_vec1, local_size, rank * 2);       // Разные seed
        generate_vector_part(local_vec2, local_size, rank * 2 + 1);
        
        // Вычисляем локальное скалярное произведение
        double local_dot = compute_local_dot_product(local_vec1, local_vec2, local_size);
        
        // Собираем глобальный результат
        double global_dot;
        MPI_Reduce(&local_dot, &global_dot, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        
        double end_time = MPI_Wtime();
        double elapsed_time = end_time - start_time;
        
        // Находим максимальное время среди всех процессов
        double max_time;
        MPI_Reduce(&elapsed_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        
        // Процесс 0 выводит результаты
        if (rank == 0) {
            printf("PARALLEL: processes=%2d, vector_size=%-10d, time=%9.6f sec, dot=%.2f\n", 
                   size, vector_size, max_time, global_dot);
        }
        
        free(local_vec1);
        free(local_vec2);
        MPI_Comm_free(&comm);
    }
    
    // Синхронизация перед следующим экспериментом
    MPI_Barrier(MPI_COMM_WORLD);
}

// Последовательная версия для сравнения
void run_sequential_experiment(int vector_size, int world_rank) {
    if (world_rank == 0) {
        printf("\n=== SEQUENTIAL DOT PRODUCT ===\n");
        
        // Тестируем разные размеры
        int seq_sizes[] = {1000, 10000, 100000, 1000000, 5000000, 10000000};
        int num_seq_tests = sizeof(seq_sizes) / sizeof(seq_sizes[0]);
        
        for (int j = 0; j < num_seq_tests; j++) {
            int size = seq_sizes[j];
            double *vec1 = (double *)malloc(size * sizeof(double));
            double *vec2 = (double *)malloc(size * sizeof(double));
            
            if (vec1 == NULL || vec2 == NULL) {
                fprintf(stderr, "Sequential: Memory allocation failed for size %d\n", size);
                continue;
            }
            
            double start_time = MPI_Wtime();
            
            // Генерация данных
            srand(time(NULL));
            for (int i = 0; i < size; i++) {
                vec1[i] = (double)rand() / RAND_MAX * 10.0;
                vec2[i] = (double)rand() / RAND_MAX * 10.0;
            }
            
            // Вычисление скалярного произведения
            double dot = 0.0;
            for (int i = 0; i < size; i++) {
                dot += vec1[i] * vec2[i];
            }
            
            double end_time = MPI_Wtime();
            
            printf("SEQUENTIAL: processes= 1, vector_size=%-10d, time=%9.6f sec\n", 
                   size, end_time - start_time);
            
            free(vec1);
            free(vec2);
        }
        printf("\n=== PARALLEL DOT PRODUCT (MPI) ===\n");
    }
}

int main(int argc, char *argv[]) {
    int world_rank, world_size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (world_rank == 0) {
        printf("=============================================================\n");
        printf("MPI Dot Product Performance Test\n");
        printf("Total available processes: %d\n", world_size);
        printf("=============================================================\n");
    }
    
    // Широкий диапазон размеров векторов для лучшего анализа ускорения
    long vector_sizes[] = {
        1000,          // Маленький
        10000,         // Средний
        100000,        // Большой
        1000000,       // 1 миллион
        5000000,       // 5 миллионов
        10000000,      // 10 миллионов
        25000000,      // 25 миллионов
        50000000       // 50 миллионов
    };
    
    int processes_to_test[] = {1, 2, 4, 8, 16, 32};
    
    int num_size_tests = sizeof(vector_sizes) / sizeof(vector_sizes[0]);
    int num_proc_tests = sizeof(processes_to_test) / sizeof(processes_to_test[0]);
    
    // Запускаем последовательную версию
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
                
                run_experiment(vec_size, num_procs, world_rank, world_size);
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
