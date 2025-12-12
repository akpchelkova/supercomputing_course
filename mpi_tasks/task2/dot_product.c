#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>        
#include <time.h>
#include <math.h>       

// функция генерации случайного вектора
void generate_vector(double *vector, int size, int seed) {
    // инициализируем генератор случайных чисел с разным seed для разных векторов
    srand(time(NULL) + seed);
    for (int i = 0; i < size; i++) {
        // заполняем вектор случайными числами от 0 до 10
        vector[i] = (double)rand() / RAND_MAX * 10.0;
    }
}

// функция вычисления локального скалярного произведения для части векторов
double compute_local_dot_product(double *vec1, double *vec2, int local_size) {
    double local_dot = 0.0;  // инициализируем локальную сумму
    // проходим по всем элементам локальных частей векторов
    for (int i = 0; i < local_size; i++) {
        // прибавляем произведение соответствующих элементов к локальной сумме
        local_dot += vec1[i] * vec2[i];
    }
    return local_dot;  // возвращаем вычисленное локальное скалярное произведение
}

// функция проведения одного эксперимента с заданными параметрами
void run_experiment(int vector_size, int use_processes, int world_rank, int world_size) {
    MPI_Comm comm;
    // определяем цвет для разделения коммуникатора: процессы с rank < use_processes попадают в группу 0
    int color = (world_rank < use_processes) ? 0 : MPI_UNDEFINED;
    // разделяем общий коммуникатор на группы
    MPI_Comm_split(MPI_COMM_WORLD, color, world_rank, &comm);
    
    // только процессы из созданной группы (color == 0) участвуют в вычислениях
    if (color == 0) {
        int rank, size;
        MPI_Comm_rank(comm, &rank);    // получаем rank в новом коммуникаторе
        MPI_Comm_size(comm, &size);    // получаем размер нового коммуникатора
        
        // начинаем замер времени выполнения
        double start_time = MPI_Wtime();
        
        // вычисляем размер локальной части векторов для текущего процесса
        // распределяем остаток от деления по первым процессам
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        // выделяем память под локальные части двух векторов
        double *local_vec1 = (double *)malloc(local_size * sizeof(double));
        double *local_vec2 = (double *)malloc(local_size * sizeof(double));
        
        // процесс с rank 0 в новом коммуникаторе генерирует и распределяет данные
        if (rank == 0) {
            // выделяем память под полные векторы
            double *full_vec1 = (double *)malloc(vector_size * sizeof(double));
            double *full_vec2 = (double *)malloc(vector_size * sizeof(double));
            // генерируем два случайных вектора с разными seed
            generate_vector(full_vec1, vector_size, 1);
            generate_vector(full_vec2, vector_size, 2);
            
            // распределяем части векторов по процессам
            int current_offset = 0;
            for (int i = 0; i < size; i++) {
                // вычисляем размер части для i-го процесса
                int proc_size = vector_size / size + (i < remainder ? 1 : 0);
                if (i == 0) {
                    // для процесса 0 просто копируем данные
                    for (int j = 0; j < proc_size; j++) {
                        local_vec1[j] = full_vec1[current_offset + j];
                        local_vec2[j] = full_vec2[current_offset + j];
                    }
                } else {
                    // для остальных процессов отправляем данные двух векторов с разными тегами
                    MPI_Send(&full_vec1[current_offset], proc_size, MPI_DOUBLE, i, 0, comm);
                    MPI_Send(&full_vec2[current_offset], proc_size, MPI_DOUBLE, i, 1, comm);
                }
                current_offset += proc_size;
            }
            // освобождаем память полных векторов
            free(full_vec1);
            free(full_vec2);
        } else {
            // процессы-получатели принимают свои части двух векторов
            MPI_Recv(local_vec1, local_size, MPI_DOUBLE, 0, 0, comm, MPI_STATUS_IGNORE);
            MPI_Recv(local_vec2, local_size, MPI_DOUBLE, 0, 1, comm, MPI_STATUS_IGNORE);
        }
        
        // вычисляем локальное скалярное произведение для своей части векторов
        double local_dot = compute_local_dot_product(local_vec1, local_vec2, local_size);
        
        // собираем глобальное скалярное произведение из всех локальных
        double global_dot;
        // операция MPI_SUM суммирует все локальные скалярные произведения
        MPI_Reduce(&local_dot, &global_dot, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        
        // замеряем время окончания вычислений
        double end_time = MPI_Wtime();
        
        // процесс 0 выводит результаты эксперимента
        if (rank == 0) {
            printf("PROCESSES: %d, SIZE: %d, TIME: %.6f\n", use_processes, vector_size, end_time - start_time);
        }
        
        // освобождаем память и коммуникатор
        free(local_vec1);
        free(local_vec2);
        MPI_Comm_free(&comm);
    }
    
    // синхронизируем все процессы перед следующим экспериментом
    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char *argv[]) {
    int world_rank, world_size;
    
    // инициализируем mpi
    MPI_Init(&argc, &argv);
    // получаем глобальный rank и размер коммуникатора
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    // процесс 0 выводит сообщение о начале экспериментов
    if (world_rank == 0) {
        printf("Starting MPI Dot Product experiments...\n");
    }
    
    // массив с количеством процессов для тестирования
    int processes_to_test[] = {1, 2, 4, 8, 16};
    // массив с размерами векторов для тестирования
    int vector_sizes[] = {1000, 10000, 100000, 1000000};
    
    // вычисляем количество тестов
    int num_proc_tests = sizeof(processes_to_test) / sizeof(processes_to_test[0]);
    int num_size_tests = sizeof(vector_sizes) / sizeof(vector_sizes[0]);
    
    // запускаем все комбинации тестов
    for (int i = 0; i < num_proc_tests; i++) {
        for (int j = 0; j < num_size_tests; j++) {
            // проверяем, что запрашиваемое количество процессов не превышает доступное
            if (processes_to_test[i] <= world_size) {
                run_experiment(vector_sizes[j], processes_to_test[i], world_rank, world_size);
            }
        }
    }
    
    // процесс 0 выводит сообщение о завершении
    if (world_rank == 0) {
        printf("All experiments completed.\n");
    }
    
    // завершаем mpi
    MPI_Finalize();
    return 0;
}
