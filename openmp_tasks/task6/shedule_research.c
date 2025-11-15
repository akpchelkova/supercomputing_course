#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>
#include <string.h>

// Функция с неравномерной вычислительной нагрузкой
// Некоторые итерации требуют больше вычислений
double heavy_computation(int iteration) {
    double result = 0.0;
    
    // Создаем неравномерную нагрузку:
    // - 10% итераций: ОЧЕНЬ тяжелые (в 100 раз больше работы)
    // - 20% итераций: тяжелые (в 10 раз больше работы)  
    // - 70% итераций: легкие (базовая нагрузка)
    
    int load_type = iteration % 10;  // от 0 до 9
    
    if (load_type == 0) {
        // ОЧЕНЬ тяжелые итерации (10%)
        for (int k = 0; k < 10000; k++) {
            result += sin(iteration * 0.001) * cos(k * 0.001);
        }
    }
    else if (load_type <= 2) {
        // Тяжелые итерации (20%)
        for (int k = 0; k < 1000; k++) {
            result += sin(iteration * 0.01) * cos(k * 0.01);
        }
    }
    else {
        // Легкие итерации (70%)
        for (int k = 0; k < 100; k++) {
            result += sin(iteration * 0.1) * cos(k * 0.1);
        }
    }
    
    return result;
}

// Функция для тестирования разных типов schedule
void test_schedule(const char* schedule_name, const char* schedule_type, int chunk_size) {
    int num_iterations = 1000;
    double total_result = 0.0;
    double start_time, end_time;
    
    printf("  %s: ", schedule_name);
    fflush(stdout);  // Немедленный вывод
    
    start_time = omp_get_wtime();
    
    if (strcmp(schedule_type, "static") == 0) {
        if (chunk_size > 0) {
            #pragma omp parallel for schedule(static, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        } else {
            #pragma omp parallel for schedule(static) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "dynamic") == 0) {
        if (chunk_size > 0) {
            #pragma omp parallel for schedule(dynamic, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        } else {
            #pragma omp parallel for schedule(dynamic) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "guided") == 0) {
        if (chunk_size > 0) {
            #pragma omp parallel for schedule(guided, chunk_size) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        } else {
            #pragma omp parallel for schedule(guided) reduction(+:total_result)
            for (int i = 0; i < num_iterations; i++) {
                total_result += heavy_computation(i);
            }
        }
    }
    else if (strcmp(schedule_type, "auto") == 0) {
        #pragma omp parallel for schedule(auto) reduction(+:total_result)
        for (int i = 0; i < num_iterations; i++) {
            total_result += heavy_computation(i);
        }
    }
    else if (strcmp(schedule_type, "runtime") == 0) {
        #pragma omp parallel for schedule(runtime) reduction(+:total_result)
        for (int i = 0; i < num_iterations; i++) {
            total_result += heavy_computation(i);
        }
    }
    
    end_time = omp_get_wtime();
    
    printf("время = %.4f сек, результат = %.2f\n", end_time - start_time, total_result);
}

// Функция для анализа распределения нагрузки
void analyze_workload() {
    printf("Анализ распределения нагрузки по итерациям:\n");
    printf("==========================================\n");
    
    int num_samples = 30;
    double times[num_samples];
    
    // Замеряем время выполнения для первых num_samples итераций
    for (int i = 0; i < num_samples; i++) {
        double start = omp_get_wtime();
        heavy_computation(i);
        double end = omp_get_wtime();
        times[i] = end - start;
    }
    
    printf("Время выполнения первых %d итераций (мс):\n", num_samples);
    for (int i = 0; i < num_samples; i++) {
        printf("%3d: %.3f | ", i, times[i] * 1000);
        if ((i + 1) % 5 == 0) printf("\n");
    }
    
    // Анализ паттерна нагрузки
    printf("\nПаттерн нагрузки:\n");
    printf("  Очень тяжелые (10%%): ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 == 0) printf("█");
    }
    printf("\n  Тяжелые (20%%):       ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 == 1 || i % 10 == 2) printf("█");
    }
    printf("\n  Легкие (70%%):        ");
    for (int i = 0; i < num_samples; i++) {
        if (i % 10 >= 3) printf("█");
    }
    printf("\n\n");
}

int main(int argc, char *argv[]) {
    int num_threads = 4;  // по умолчанию 4 потока
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    }
    
    omp_set_num_threads(num_threads);
    
    printf("ИССЛЕДОВАНИЕ РЕЖИМОВ РАСПАРАЛЛЕЛИВАНИЯ ЦИКЛА for\n");
    printf("================================================\n");
    printf("Количество потоков: %d\n", num_threads);
    printf("Количество итераций: 1000\n");
    printf("Распределение нагрузки:\n");
    printf("  - 10%% итераций: ОЧЕНЬ тяжелые (в 100 раз больше работы)\n");
    printf("  - 20%% итераций: тяжелые (в 10 раз больше работы)\n");
    printf("  - 70%% итераций: легкие (базовая нагрузка)\n\n");
    
    // Анализ нагрузки
    analyze_workload();
    
    printf("РЕЗУЛЬТАТЫ ЭКСПЕРИМЕНТОВ:\n");
    printf("=========================\n");
    
    // Базовые тесты без chunk size
    printf("Базовые schedule типы:\n");
    test_schedule("static (default)", "static", 0);
    test_schedule("dynamic (default)", "dynamic", 0);
    test_schedule("guided (default)", "guided", 0);
    
    // Static с разным chunk size
    printf("\nStatic с разным chunk size:\n");
    test_schedule("static, chunk=1", "static", 1);
    test_schedule("static, chunk=10", "static", 10);
    test_schedule("static, chunk=50", "static", 50);
    test_schedule("static, chunk=100", "static", 100);
    
    // Dynamic с разным chunk size
    printf("\nDynamic с разным chunk size:\n");
    test_schedule("dynamic, chunk=1", "dynamic", 1);
    test_schedule("dynamic, chunk=10", "dynamic", 10);
    test_schedule("dynamic, chunk=50", "dynamic", 50);
    test_schedule("dynamic, chunk=100", "dynamic", 100);
    
    // Guided с разным chunk size
    printf("\nGuided с разным chunk size:\n");
    test_schedule("guided, chunk=1", "guided", 1);
    test_schedule("guided, chunk=10", "guided", 10);
    test_schedule("guided, chunk=50", "guided", 50);
    
    // Auto и runtime
    printf("\nДругие schedule типы:\n");
    test_schedule("auto", "auto", 0);
    test_schedule("runtime", "runtime", 0);
    
    printf("\nВЫВОД: Лучший результат выделен\n");
    
    return 0;
}
