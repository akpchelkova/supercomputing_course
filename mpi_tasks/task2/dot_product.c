#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <math.h>

// Генерация случайного вектора
void generate_vector(double *vector, int size, int seed) {
    srand(time(NULL) + seed);
    for (int i = 0; i < size; i++) {
        vector[i] = (double)rand() / RAND_MAX * 10.0; // Числа от 0 до 10
    }
}

// Вычисление локального скалярного произведения
double compute_local_dot_product(double *vec1, double *vec2, int local_size) {
    double local_dot = 0.0;
    for (int i = 0; i < local_size; i++) {
        local_dot += vec1[i] * vec2[i];
    }
    return local_dot;
}

// Проверка корректности (последовательная версия)
double sequential_dot_product(double *vec1, double *vec2, int size) {
    double result = 0.0;
    for (int i = 0; i < size; i++) {
        result += vec1[i] * vec2[i];
    }
    return result;
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    
    // Диагностика распределения процессов по узлам
    if (rank == 0) {
        printf("=== MPI Dot Product Computation ===\n");
        printf("Total processes: %d\n", size);
        
        // Собираем информацию о всех узлах
        printf("Node distribution:\n");
        printf("Process %d: %s\n", rank, processor_name);  // себя выводим сразу
        
        for (int i = 1; i < size; i++) {
            char other_name[MPI_MAX_PROCESSOR_NAME];
            MPI_Recv(other_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Process %d: %s\n", i, other_name);
        }
        printf("\n");
    } else {
        MPI_Send(processor_name, name_len + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }
    
    // Параметры тестирования
    int vector_sizes[] = {1000, 10000, 100000, 1000000};
    int num_sizes = sizeof(vector_sizes) / sizeof(vector_sizes[0]);
    
    for (int v_idx = 0; v_idx < num_sizes; v_idx++) {
        int vector_size = vector_sizes[v_idx];
        
        double start_time = MPI_Wtime();
        
        // Распределение данных по процессам
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        double *local_vec1 = (double *)malloc(local_size * sizeof(double));
        double *local_vec2 = (double *)malloc(local_size * sizeof(double));
        
        if (rank == 0) {
            // Генерация полных векторов
            double *full_vec1 = (double *)malloc(vector_size * sizeof(double));
            double *full_vec2 = (double *)malloc(vector_size * sizeof(double));
            generate_vector(full_vec1, vector_size, 1);
            generate_vector(full_vec2, vector_size, 2);
            
            // Распределение данных
            int current_offset = 0;
            for (int i = 0; i < size; i++) {
                int proc_size = vector_size / size + (i < remainder ? 1 : 0);
                if (i == 0) {
                    // Копируем свою часть
                    for (int j = 0; j < proc_size; j++) {
                        local_vec1[j] = full_vec1[current_offset + j];
                        local_vec2[j] = full_vec2[current_offset + j];
                    }
                } else {
                    // Отправляем другим процессам
                    MPI_Send(&full_vec1[current_offset], proc_size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    MPI_Send(&full_vec2[current_offset], proc_size, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
                }
                current_offset += proc_size;
            }
            
            free(full_vec1);
            free(full_vec2);
        } else {
            // Получение данных от процесса 0
            MPI_Recv(local_vec1, local_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(local_vec2, local_size, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        // Вычисление локального скалярного произведения
        double local_dot = compute_local_dot_product(local_vec1, local_vec2, local_size);
        
        // Сбор результатов со всех процессов
        double global_dot;
        MPI_Reduce(&local_dot, &global_dot, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        
        double end_time = MPI_Wtime();
        
        if (rank == 0) {
            // Проверка корректности на маленьком размере
            int check_size = 1000;
            double *check_vec1 = (double *)malloc(check_size * sizeof(double));
            double *check_vec2 = (double *)malloc(check_size * sizeof(double));
            generate_vector(check_vec1, check_size, 1);
            generate_vector(check_vec2, check_size, 2);
            double check_result = sequential_dot_product(check_vec1, check_vec2, check_size);
            
            printf("Vector size: %8d | Processes: %2d | Time: %.6f sec\n", 
                   vector_size, size, end_time - start_time);
            printf("Dot product result: %.6f\n", global_dot);
            printf("Correctness check (size %d): %.6f\n", check_size, check_result);
            printf("---\n");
            
            free(check_vec1);
            free(check_vec2);
        }
        
        free(local_vec1);
        free(local_vec2);
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        printf("All dot product experiments completed!\n");
    }
    
    MPI_Finalize();
    return 0;
}
