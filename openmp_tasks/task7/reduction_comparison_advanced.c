#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Инициализация массива случайными числами
void initialize_array(double *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = (double)rand() / RAND_MAX * 100.0;
    }
}

// 1. РЕДУКЦИЯ С ПОМОЩЬЮ REDUCTION
double reduction_reduction(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum;
}

// 2. РЕДУКЦИЯ С КРИТИЧЕСКИМИ СЕКЦИЯМИ
double reduction_critical(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel
    {
        double local_sum = 0.0;
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sum += arr[i];
        }
        #pragma omp critical
        {
            sum += local_sum;
        }
    }
    return sum;
}

// 3. РЕДУКЦИЯ С АТОМАРНЫМИ ОПЕРАЦИЯМИ
double reduction_atomic(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        #pragma omp atomic
        sum += arr[i];
    }
    return sum;
}

// 4. РЕДУКЦИЯ С ЗАМКАМИ (lock)
double reduction_locks(double *arr, int size) {
    double sum = 0.0;
    omp_lock_t lock;
    omp_init_lock(&lock);
    
    #pragma omp parallel
    {
        double local_sum = 0.0;
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sum += arr[i];
        }
        
        omp_set_lock(&lock);
        sum += local_sum;
        omp_unset_lock(&lock);
    }
    
    omp_destroy_lock(&lock);
    return sum;
}

// 5. РЕДУКЦИЯ С МАССИВОМ ЛОКАЛЬНЫХ ПЕРЕМЕННЫХ
double reduction_local_array(double *arr, int size) {
    double sum = 0.0;
    int num_threads = omp_get_max_threads();
    double *local_sums = (double*)calloc(num_threads, sizeof(double));
    
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sums[thread_id] += arr[i];
        }
    }
    
    for (int i = 0; i < num_threads; i++) {
        sum += local_sums[i];
    }
    
    free(local_sums);
    return sum;
}

// 6. РЕДУКЦИЯ С ОДНОВРЕМЕННЫМИ АТОМАРНЫМИ ОПЕРАЦИЯМИ (худший случай)
double reduction_atomic_bad(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        #pragma omp atomic
        sum += arr[i];
    }
    return sum;
}

// Функция для измерения времени одного метода
double measure_time(const char* method_name, double (*func)(double*, int), 
                   double *array, int size, double reference_result, int verbose) {
    double start_time, end_time, result;
    
    // "Прогрев" кэша
    func(array, 1000);
    
    start_time = omp_get_wtime();
    result = func(array, size);
    end_time = omp_get_wtime();
    
    double error = fabs(result - reference_result);
    double time_taken = end_time - start_time;
    
    if (verbose) {
        printf("  %-25s: время = %.6f сек, ошибка = %.10f", 
               method_name, time_taken, error);
        
        if (error > 1e-6) {
            printf(" ⚠️  ОШИБКА!");
        }
        printf("\n");
    }
    
    return time_taken;
}

int main(int argc, char *argv[]) {
    int default_size = 10000000;
    int size = default_size;
    int num_threads = 4;
    int verbose = 1;
    int csv_mode = 0;
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) {
            num_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i+1 < argc) {
            size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv_mode = 1;
            verbose = 0;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            verbose = 0;
        }
    }
    
    omp_set_num_threads(num_threads);
    
    // Выделение памяти и инициализация
    double *array = (double*)malloc(size * sizeof(double));
    srand(time(NULL));
    initialize_array(array, size);
    
    if (verbose) {
        printf("СРАВНЕНИЕ СПОСОБОВ РЕДУКЦИИ\n");
        printf("============================\n");
        printf("Размер массива: %d\n", size);
        printf("Количество потоков: %d\n", num_threads);
        printf("Тип операции: суммирование\n\n");
    }
    
    // Вычисляем эталонное значение (последовательно)
    double reference_sum = 0.0;
    for (int i = 0; i < size; i++) {
        reference_sum += array[i];
    }
    
    if (verbose) {
        printf("Эталонная сумма: %.2f\n\n", reference_sum);
        printf("РЕЗУЛЬТАТЫ ЭКСПЕРИМЕНТОВ:\n");
        printf("=========================\n");
    }
    
    // Массив методов для тестирования
    struct Method {
        const char* name;
        double (*function)(double*, int);
    } methods[] = {
        {"reduction", reduction_reduction},
        {"critical", reduction_critical},
        {"atomic", reduction_atomic},
        {"locks", reduction_locks},
        {"local_array", reduction_local_array},
        {"atomic_bad", reduction_atomic_bad}
    };
    
    int num_methods = sizeof(methods) / sizeof(methods[0]);
    double times[num_methods];
    
    // Тестируем все методы
    for (int i = 0; i < num_methods; i++) {
        times[i] = measure_time(methods[i].name, methods[i].function, 
                               array, size, reference_sum, verbose);
    }
    
    // Вывод в CSV формате
    if (csv_mode) {
        printf("%d,%d", num_threads, size);
        for (int i = 0; i < num_methods; i++) {
            printf(",%.6f", times[i]);
        }
        printf("\n");
    }
    
    // Анализ производительности
    if (verbose) {
        printf("\nАНАЛИЗ ПРОИЗВОДИТЕЛЬНОСТИ:\n");
        printf("==========================\n");
        
        double base_time = times[0]; // reduction как базовое
        printf("Базовое время (reduction): %.6f сек\n", base_time);
        printf("Относительная производительность:\n");
        
        for (int i = 1; i < num_methods; i++) {
            double ratio = base_time / times[i];
            printf("  %-15s: %.2fx %s\n", methods[i].name, ratio,
                   ratio >= 1.0 ? "✅" : "❌");
        }
    }
    
    free(array);
    return 0;
}
