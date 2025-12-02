#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h>
#include <time.h>
#include <math.h>

// инициализация массива случайными числами
void initialize_array(double *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = (double)rand() / RAND_MAX * 100.0;  // случайные числа от 0 до 100
    }
}

// 1. редукция с помощью reduction (самый эффективный способ)
double reduction_reduction(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)  // openmp автоматически управляет редукцией
    for (int i = 0; i < size; i++) {
        sum += arr[i];  // каждый поток накапливает свою локальную сумму
    }
    return sum;  // openmp автоматически объединяет результаты потоков
}

// 2. редукция с критическими секциями
double reduction_critical(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel
    {
        double local_sum = 0.0;  // локальная сумма для каждого потока
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sum += arr[i];  // поток обрабатывает свою часть массива
        }
        // критическая секция для безопасного обновления глобальной суммы
        #pragma omp critical
        {
            sum += local_sum;  // только один поток может выполнять эту операцию одновременно
        }
    }
    return sum;
}

// 3. редукция с атомарными операциями
double reduction_atomic(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        #pragma omp atomic  // атомарная операция - безопасное обновление без блокировок
        sum += arr[i];  // каждый элемент добавляется атомарно
    }
    return sum;
}

// 4. редукция с замками (lock)
double reduction_locks(double *arr, int size) {
    double sum = 0.0;
    omp_lock_t lock;  // создаем замок
    omp_init_lock(&lock);  // инициализируем замок
    
    #pragma omp parallel
    {
        double local_sum = 0.0;  // локальная сумма для потока
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sum += arr[i];  // накапливаем локально
        }
        
        // используем замок для защиты глобальной переменной
        omp_set_lock(&lock);  // захватываем замок
        sum += local_sum;  // безопасно обновляем глобальную сумму
        omp_unset_lock(&lock);  // освобождаем замок
    }
    
    omp_destroy_lock(&lock);  // уничтожаем замок
    return sum;
}

// 5. редукция с массивом локальных переменных
double reduction_local_array(double *arr, int size) {
    double sum = 0.0;
    int num_threads = omp_get_max_threads();  // получаем максимальное количество потоков
    double *local_sums = (double*)calloc(num_threads, sizeof(double));  // массив для локальных сумм
    
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();  // получаем id текущего потока
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_sums[thread_id] += arr[i];  // каждый поток пишет в свою ячейку массива
        }
    }
    
    // последовательно суммируем все локальные суммы
    for (int i = 0; i < num_threads; i++) {
        sum += local_sums[i];
    }
    
    free(local_sums);  // освобождаем память
    return sum;
}

// 6. редукция с одновременными атомарными операциями (худший случай)
double reduction_atomic_bad(double *arr, int size) {
    double sum = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        #pragma omp atomic  // атомарная операция для каждого элемента - много конфликтов
        sum += arr[i];  // очень неэффективно из-за постоянной синхронизации
    }
    return sum;
}

// функция для измерения времени одного метода
double measure_time(const char* method_name, double (*func)(double*, int), 
                   double *array, int size, double reference_result, int verbose) {
    double start_time, end_time, result;
    
    // "прогрев" кэша - выполняем метод на маленьком массиве чтобы прогреть кэш
    func(array, 1000);
    
    start_time = omp_get_wtime();  // засекаем время начала
    result = func(array, size);  // выполняем метод на полном массиве
    end_time = omp_get_wtime();  // засекаем время окончания
    
    double error = fabs(result - reference_result);  // вычисляем ошибку относительно эталона
    double time_taken = end_time - start_time;  // вычисляем время выполнения
    
    if (verbose) {
        printf("  %-25s: время = %.6f сек, ошибка = %.10f", 
               method_name, time_taken, error);
        
        if (error > 1e-6) {  // если ошибка значительная - выводим предупреждение
            printf("ошибкаа!");
        }
        printf("\n");
    }
    
    return time_taken;
}

int main(int argc, char *argv[]) {
    int default_size = 10000000;  // размер массива по умолчанию - 10 миллионов
    int size = default_size;
    int num_threads = 4;  // количество потоков по умолчанию
    int verbose = 1;  // режим подробного вывода
    int csv_mode = 0;  // режим вывода в csv формате
    
    // парсинг аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) {
            num_threads = atoi(argv[++i]);  // устанавливаем количество потоков
        } else if (strcmp(argv[i], "--size") == 0 && i+1 < argc) {
            size = atoi(argv[++i]);  // устанавливаем размер массива
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv_mode = 1;  // включаем csv режим
            verbose = 0;   // отключаем подробный вывод
        } else if (strcmp(argv[i], "--quiet") == 0) {
            verbose = 0;  // отключаем подробный вывод
        }
    }
    
    omp_set_num_threads(num_threads);  // устанавливаем количество потоков для openmp
    
    // выделение памяти и инициализация массива
    double *array = (double*)malloc(size * sizeof(double));
    srand(time(NULL));  // инициализация генератора случайных чисел
    initialize_array(array, size);  // заполнение массива случайными числами
    
    if (verbose) {
        printf("сравнение способов редукции\n");
        printf("============================\n");
        printf("размер массива: %d\n", size);
        printf("количество потоков: %d\n", num_threads);
        printf("тип операции: суммирование\n\n");
    }
    
    // вычисляем эталонное значение (последовательно для проверки корректности)
    double reference_sum = 0.0;
    for (int i = 0; i < size; i++) {
        reference_sum += array[i];
    }
    
    if (verbose) {
        printf("эталонная сумма: %.2f\n\n", reference_sum);
        printf("результаты экспериментов:\n");
        printf("=========================\n");
    }
    
    // массив методов для тестирования
    struct Method {
        const char* name;  // название метода
        double (*function)(double*, int);  // указатель на функцию
    } methods[] = {
        {"reduction", reduction_reduction},  // стандартная редукция openmp
        {"critical", reduction_critical},    // критические секции
        {"atomic", reduction_atomic},        // атомарные операции
        {"locks", reduction_locks},          // замки
        {"local_array", reduction_local_array},  // массив локальных переменных
        {"atomic_bad", reduction_atomic_bad}     // неэффективные атомарные операции
    };
    
    int num_methods = sizeof(methods) / sizeof(methods[0]);  // количество методов
    double times[num_methods];  // массив для хранения времени выполнения
    
    // тестируем все методы
    for (int i = 0; i < num_methods; i++) {
        times[i] = measure_time(methods[i].name, methods[i].function, 
                               array, size, reference_sum, verbose);
    }
    
    // вывод в csv формате для последующего анализа
    if (csv_mode) {
        printf("%d,%d", num_threads, size);  // выводим параметры эксперимента
        for (int i = 0; i < num_methods; i++) {
            printf(",%.6f", times[i]);  // выводим время для каждого метода
        }
        printf("\n");
    }
    
    // анализ производительности методов
    if (verbose) {
        printf("\nанализ производительности:\n");
        printf("==========================\n");
        
        double base_time = times[0]; // берем reduction как базовое время
        printf("базовое время (reduction): %.6f сек\n", base_time);
        printf("относительная производительность:\n");
        
        // сравниваем производительность всех методов с reduction
        for (int i = 1; i < num_methods; i++) {
            double ratio = base_time / times[i];  // во сколько раз метод быстрее/медленнее
            printf("  %-15s: %.2fx %s\n", methods[i].name, ratio,
                   ratio >= 1.0 ? "+" : "-");  // показываем успешность метода
        }
    }
    
    free(array);  // освобождаем память
    return 0;
}
