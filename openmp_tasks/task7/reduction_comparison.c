#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>

#define ARRAY_SIZE 10000000

// Инициализация массива случайными числами
void initialize_array(double *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = (double)rand() / RAND_MAX * 100.0;
    }
}

// 1. РЕДУКЦИЯ С ПОМОЩЬЮ REDUCTION (базовый способ)
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
        // Атомарная операция на КАЖДОЙ итерации - очень медленно!
        #pragma omp atomic
        sum += arr[i];
    }
    return sum;
}

// Функция для тестирования одного метода
void test_method(const char* method_name, double (*func)(double*, int), 
                 double *array, int size, double reference_result) {
    double start_time, end_time, result;
    
    // "Прогрев" кэша
    func(array, 1000);
    
    start_time = omp_get_wtime();
    result = func(array, size);
    end_time = omp_get_wtime();
    
    double error = fabs(result - reference_result);
    printf("  %-25s: время = %.4f сек, ошибка = %.10f", 
           method_name, end_time - start_time, error);
    
    // Проверка корректности
    if (error > 1e-6) {
        printf(" ⚠️  ОШИБКА!");
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    int size = ARRAY_SIZE;
    int num_threads = 4;
    
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    }
    
    omp_set_num_threads(num_threads);
    
    // Выделение памяти и инициализация
    double *array = (double*)malloc(size * sizeof(double));
    srand(time(NULL));
    initialize_array(array, size);
    
    printf("СРАВНЕНИЕ СПОСОБОВ РЕДУКЦИИ\n");
    printf("============================\n");
    printf("Размер массива: %d\n", size);
    printf("Количество потоков: %d\n", num_threads);
    printf("Тип операции: суммирование\n\n");
    
    // Вычисляем эталонное значение (последовательно)
    double reference_sum = 0.0;
    for (int i = 0; i < size; i++) {
        reference_sum += array[i];
    }
    printf("Эталонная сумма: %.2f\n\n", reference_sum);
    
    printf("РЕЗУЛЬТАТЫ ЭКСПЕРИМЕНТОВ:\n");
    printf("=========================\n");
    
    // Тестируем все методы
    test_method("reduction", reduction_reduction, array, size, reference_sum);
    test_method("critical sections", reduction_critical, array, size, reference_sum);
    test_method("atomic (локальный)", reduction_atomic, array, size, reference_sum);
    test_method("locks", reduction_locks, array, size, reference_sum);
    test_method("local array", reduction_local_array, array, size, reference_sum);
    test_method("atomic (на каждой итерации)", reduction_atomic_bad, array, size, reference_sum);
    
    // Дополнительный анализ
    printf("\nАНАЛИЗ ПРОИЗВОДИТЕЛЬНОСТИ:\n");
    printf("==========================\n");
    
    // Замеряем время для reduction (как базовое)
    double base_time;
    double start = omp_get_wtime();
    reduction_reduction(array, size);
    double end = omp_get_wtime();
    base_time = end - start;
    
    printf("Базовое время (reduction): %.4f сек\n", base_time);
    printf("Относительная производительность:\n");
    
    // Сравниваем все методы относительно reduction
    double methods_time;
    start = omp_get_wtime();
    reduction_critical(array, size);
    end = omp_get_wtime();
    methods_time = end - start;
    printf("  critical sections: %.2fx %s\n", base_time/methods_time, 
           base_time/methods_time >= 1.0 ? "✅" : "❌");
    
    start = omp_get_wtime();
    reduction_atomic(array, size);
    end = omp_get_wtime();
    methods_time = end - start;
    printf("  atomic: %.2fx %s\n", base_time/methods_time,
           base_time/methods_time >= 1.0 ? "✅" : "❌");
    
    start = omp_get_wtime();
    reduction_locks(array, size);
    end = omp_get_wtime();
    methods_time = end - start;
    printf("  locks: %.2fx %s\n", base_time/methods_time,
           base_time/methods_time >= 1.0 ? "✅" : "❌");
    
    start = omp_get_wtime();
    reduction_local_array(array, size);
    end = omp_get_wtime();
    methods_time = end - start;
    printf("  local array: %.2fx %s\n", base_time/methods_time,
           base_time/methods_time >= 1.0 ? "✅" : "❌");
    
    start = omp_get_wtime();
    reduction_atomic_bad(array, size);
    end = omp_get_wtime();
    methods_time = end - start;
    printf("  atomic (bad): %.2fx %s\n", base_time/methods_time,
           base_time/methods_time >= 1.0 ? "✅" : "❌");
    
    free(array);
    return 0;
}
