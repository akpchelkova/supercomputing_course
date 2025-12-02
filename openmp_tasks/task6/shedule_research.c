#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>
#include <string.h>

// функция с неравномерной вычислительной нагрузкой
// некоторые итерации требуют больше вычислений
double heavy_computation(int iteration) {
    double result = 0.0;
    
    // создаем неравномерную нагрузку:
    // - 10% итераций: очень тяжелые (в 100 раз больше работы)
    // - 20% итераций: тяжелые (в 10 раз больше работы)  
    // - 70% итераций: легкие (базовая нагрузка)
    
    int load_type = iteration % 10;  // определяем тип нагрузки по остатку от деления
    
    if (load_type == 0) {
        // очень тяжелые итерации (10%) - в 100 раз больше вычислений
        for (int k = 0; k < 10000; k++) {
            result += sin(iteration * 0.001) * cos(k * 0.001);  // тяжелые тригонометрические вычисления
        }
    }
    else if (load_type <= 2) {
        // тяжелые итерации (20%) - в 10 раз больше вычислений
        for (int k = 0; k < 1000; k++) {
            result += sin(iteration * 0.01) * cos(k * 0.01);  // умеренные вычисления
        }
    }
    else {
        // легкие итерации (70%) - базовая нагрузка
        for (int k = 0; k < 100; k++) {
            result += sin(iteration * 0.1) * cos(k * 0.1);  // легкие вычисления
        }
    }
    
    return result;
}

// функция для тестирования разных типов schedule
void test_schedule(const char* schedule_name, const char* schedule_type, int chunk_size) {
    int num_iterations = 1000;  // общее количество итераций
    double total_result = 0.0;  // переменная для накопления результата
    double start_time, end_time;  // переменные для измерения времени
    
    printf("  %s: ", schedule_name);
    fflush(stdout);  // немедленный вывод чтобы видеть прогресс
    
    start_time = omp_get_wtime();  // засекаем время начала
    
    // в зависимости от типа schedule применяем соответствующую директиву openmp
    if (strcmp(schedule_type, "static") == 0) {
        if (chunk_size > 0) {
            // static с указанным размером блока
            #pragma omp parallel for schedule(static, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);  // каждая итерация вносит вклад в результат
            }
        } else {
            // static с автоматическим определением размера блока
            #pragma omp parallel for schedule(static) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "dynamic") == 0) {
        if (chunk_size > 0) {
            // dynamic с указанным размером порции
            #pragma omp parallel for schedule(dynamic, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        } else {
            // dynamic с размером порции по умолчанию
            #pragma omp parallel for schedule(dynamic) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "guided") == 0) {
        if (chunk_size > 0) {
            // guided с минимальным размером порции
            #pragma omp parallel for schedule(guided, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        } else {
            // guided с минимальным размером порции по умолчанию
            #pragma omp parallel for schedule(guided) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "auto") == 0) {
        // автоматический выбор schedule компилятором
        #pragma omp parallel for schedule(auto) reduction(+:total_result)
        for (int i = 0; i < num_iterations; i++) {
            total_result += heavy_computation(i);
        }
    }
    else if (strcmp(schedule_type, "runtime") == 0) {
        // выбор schedule во время выполнения через переменную окружения
        #pragma omp parallel for schedule(runtime) reduction(+:total_result)
        for (int i = 0; i < num_iterations; i++) {
            total_result += heavy_computation(i);
        }
    }
    
    end_time = omp_get_wtime();  // засекаем время окончания
    
    printf("время = %.4f сек, результат = %.2f\n", end_time - start_time, total_result);
}

// функция для анализа распределения нагрузки по итерациям
void analyze_workload() {
    printf("анализ распределения нагрузки по итерациям:\n");
    printf("==========================================\n");
    
    int num_samples = 30;  // количество итераций для анализа
    double times[num_samples];  // массив для хранения времени выполнения
    
    // замеряем время выполнения для первых num_samples итераций
    for (int i = 0; i < num_samples; i++) {
        double start = omp_get_wtime();  // время начала итерации
        heavy_computation(i);  // выполняем вычисления
        double end = omp_get_wtime();  // время окончания итерации
        times[i] = end - start;  // сохраняем время выполнения
    }
    
    printf("время выполнения первых %d итераций (мс):\n", num_samples);
    for (int i = 0; i < num_samples; i++) {
        printf("%3d: %.3f | ", i, times[i] * 1000);  // выводим время в миллисекундах
        if ((i + 1) % 5 == 0) printf("\n");  // перенос строки каждые 5 итераций
    }
    
    // анализ паттерна нагрузки с визуализацией
    printf("\nпаттерн нагрузки:\n");
    printf("  очень тяжелые (10%%): ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 == 0) printf("VH");  // визуализация очень тяжелых итераций
    }
    printf("\n  тяжелые (20%%):       ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 == 1 || i % 10 == 2) printf("H");  // визуализация тяжелых итераций
    }
    printf("\n  легкие (70%%):        ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 >= 3) printf("L");  // визуализация легких итераций
    }
    printf("\n\n");
}

int main(int argc, char *argv[]) {
    int num_threads = 4;  // количество потоков по умолчанию
    if (argc > 1) {
        num_threads = atoi(argv[1]);  // можно передать количество потоков как аргумент
    }
    
    omp_set_num_threads(num_threads);  // устанавливаем количество потоков для openmp
    
    printf("исследование режимов распараллеливания цикла for\n");
    printf("================================================\n");
    printf("количество потоков: %d\n", num_threads);
    printf("количество итераций: 1000\n");
    printf("распределение нагрузки:\n");
    printf("  - 10%% итераций: очень тяжелые (в 100 раз больше работы)\n");
    printf("  - 20%% итераций: тяжелые (в 10 раз больше работы)\n");
    printf("  - 70%% итераций: легкие (базовая нагрузка)\n\n");
    
    // анализ нагрузки для понимания распределения
    analyze_workload();
    
    printf("результаты экспериментов:\n");
    printf("=========================\n");
    
    // базовые тесты без указания chunk size
    printf("базовые schedule типы:\n");
    test_schedule("static (default)", "static", 0);
    test_schedule("dynamic (default)", "dynamic", 0);
    test_schedule("guided (default)", "guided", 0);
    
    // static с разным размером блока (chunk size)
    printf("\nstatic с разным chunk size:\n");
    test_schedule("static, chunk=1", "static", 1);
    test_schedule("static, chunk=10", "static", 10);
    test_schedule("static, chunk=50", "static", 50);
    test_schedule("static, chunk=100", "static", 100);
    
    // dynamic с разным размером порции
    printf("\ndynamic с разным chunk size:\n");
    test_schedule("dynamic, chunk=1", "dynamic", 1);
    test_schedule("dynamic, chunk=10", "dynamic", 10);
    test_schedule("dynamic, chunk=50", "dynamic", 50);
    test_schedule("dynamic, chunk=100", "dynamic", 100);
    
    // guided с разным минимальным размером порции
    printf("\nguided с разным chunk size:\n");
    test_schedule("guided, chunk=1", "guided", 1);
    test_schedule("guided, chunk=10", "guided", 10);
    test_schedule("guided, chunk=50", "guided", 50);
    
    // auto и runtime - специальные типы распределения
    printf("\nдругие schedule типы:\n");
    test_schedule("auto", "auto", 0);
    test_schedule("runtime", "runtime", 0);
    
    printf("\nвывод: лучший результат выделен\n");
    
    return 0;
}
