#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>     
#include <time.h>
#include <limits.h>

// функция генерации случайного вектора
void generate_vector(double *vector, int size, int rank) {
    // инициализируем генератор случайных чисел с разным seed для каждого процесса (добавляем rank)
    srand(time(NULL) + rank);
    for (int i = 0; i < size; i++) {
        // заполняем вектор случайными числами от 0 до 1000
        vector[i] = (double)rand() / RAND_MAX * 1000.0;
    }
}

// функция поиска локального минимума и максимума в части вектора
void find_local_min_max(double *vector, int local_size, double *local_min, double *local_max) {
    // инициализируем минимум и максимум первым элементом
    *local_min = vector[0];
    *local_max = vector[0];
    
    // проходим по всем элементам локальной части вектора
    for (int i = 1; i < local_size; i++) {
        if (vector[i] < *local_min) *local_min = vector[i];  // обновляем минимум если нашли меньший элемент
        if (vector[i] > *local_max) *local_max = vector[i];  // обновляем максимум если нашли больший элемент
    }
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
        
        // вычисляем размер локальной части вектора для текущего процесса
        // распределяем остаток от деления по первым процессам
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        // выделяем память под локальную часть вектора
        double *local_vector = (double *)malloc(local_size * sizeof(double));
        
        // процесс с rank 0 в новом коммуникаторе генерирует и распределяет данные
        if (rank == 0) {
            // выделяем память под весь вектор
            double *full_vector = (double *)malloc(vector_size * sizeof(double));
            // генерируем весь вектор
            generate_vector(full_vector, vector_size, rank);
            
            // распределяем части вектора по процессам
            int current_offset = 0;
            for (int i = 0; i < size; i++) {
                // вычисляем размер части для i-го процесса
                int proc_size = vector_size / size + (i < remainder ? 1 : 0);
                if (i == 0) {
                    // для процесса 0 просто копируем данные
                    for (int j = 0; j < proc_size; j++) {
                        local_vector[j] = full_vector[current_offset + j];
                    }
                } else {
                    // для остальных процессов отправляем данные
                    MPI_Send(&full_vector[current_offset], proc_size, MPI_DOUBLE, i, 0, comm);
                }
                current_offset += proc_size;
            }
            free(full_vector);
        } else {
            // процессы-получатели принимают свою часть вектора
            MPI_Recv(local_vector, local_size, MPI_DOUBLE, 0, 0, comm, MPI_STATUS_IGNORE);
        }
        
        // находим локальные минимум и максимум в своей части вектора
        double local_min, local_max;
        find_local_min_max(local_vector, local_size, &local_min, &local_max);
        
        // собираем глобальные минимум и максимум из всех локальных
        double global_min, global_max;
        // операция MPI_MIN найдет минимальное значение из всех переданных
        MPI_Reduce(&local_min, &global_min, 1, MPI_DOUBLE, MPI_MIN, 0, comm);
        // операция MPI_MAX найдет максимальное значение из всех переданных
        MPI_Reduce(&local_max, &global_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        
        // замеряем время окончания вычислений
        double end_time = MPI_Wtime();
        
        // процесс 0 выводит результаты эксперимента
        if (rank == 0) {
            printf("EXPERIMENT: processes=%d, vector_size=%d, time=%.6f\n", use_processes, vector_size, end_time - start_time);
            printf("RESULTS: min=%.2f, max=%.2f\n", global_min, global_max);
            printf("---\n");
        }
        
        // освобождаем память и коммуникатор
        free(local_vector);
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
        printf("Starting MPI experiments...\n");
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
