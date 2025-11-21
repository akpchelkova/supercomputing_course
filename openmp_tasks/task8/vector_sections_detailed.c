#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// Параметры которые будем менять в экспериментах
int NUM_VECTORS = 8;
int VECTOR_SIZE = 1000000;
int NUM_THREADS = 3;

// Глобальные флаги для синхронизации
int vectors_loaded = 0;
int computations_done = 0;

// Структура для данных
typedef struct {
    double *vectors_a;
    double *vectors_b;
    double *results;
} SharedData;

// Генерация тестовых данных
void generate_test_data() {
    FILE *file_a, *file_b;
    
    printf("Генерация тестовых данных...\n");
    
    // Создаем файл с векторами A
    file_a = fopen("vectors_a.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_a);
        }
    }
    fclose(file_a);
    
    // Создаем файл с векторами B  
    file_b = fopen("vectors_b.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_b);
        }
    }
    fclose(file_b);
    
    printf("Данные сгенерированы: %d векторов по %d элементов\n", NUM_VECTORS, VECTOR_SIZE);
}

// ЗАДАЧА 1: Чтение векторов из файла
void task_read_vectors(SharedData *data) {
    FILE *file;
    
    // Чтение vectors_a.dat
    file = fopen("vectors_a.dat", "rb");
    if (!file) {
        printf("Ошибка: не могу открыть vectors_a.dat\n");
        return;
    }
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&data->vectors_a[i * VECTOR_SIZE + j], sizeof(double), 1, file);
        }
    }
    fclose(file);
    
    // Чтение vectors_b.dat
    file = fopen("vectors_b.dat", "rb");
    if (!file) {
        printf("Ошибка: не могу открыть vectors_b.dat\n");
        return;
    }
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&data->vectors_b[i * VECTOR_SIZE + j], sizeof(double), 1, file);
        }
    }
    fclose(file);
    
    // Устанавливаем флаг загрузки
    #pragma omp atomic write
    vectors_loaded = 1;
    #pragma omp flush(vectors_loaded)
}

// Функция вычисления скалярного произведения для одного вектора
double dot_product(double *a, double *b, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ЗАДАЧА 2: Вычисление скалярных произведений
void task_compute_products(SharedData *data) {
    // Ожидаем загрузки данных
    while (1) {
        #pragma omp flush(vectors_loaded)
        if (vectors_loaded) break;
        for (int i = 0; i < 1000; i++) {} // Короткая пауза
    }
    
    // Вычисление скалярных произведений
    for (int i = 0; i < NUM_VECTORS; i++) {
        double *vec_a = &data->vectors_a[i * VECTOR_SIZE];
        double *vec_b = &data->vectors_b[i * VECTOR_SIZE];
        data->results[i] = dot_product(vec_a, vec_b, VECTOR_SIZE);
    }
    
    // Устанавливаем флаг завершения вычислений
    #pragma omp atomic write
    computations_done = 1;
    #pragma omp flush(computations_done)
}

// ЗАДАЧА 3: Сохранение результатов
void task_save_results(SharedData *data) {
    // Ожидаем завершения вычислений
    while (1) {
        #pragma omp flush(computations_done)
        if (computations_done) break;
        for (int i = 0; i < 1000; i++) {} // Короткая пауза
    }
    
    FILE *file = fopen("results.dat", "w");
    if (!file) {
        printf("Ошибка: не могу создать results.dat\n");
        return;
    }
    
    for (int i = 0; i < NUM_VECTORS; i++) {
        fprintf(file, "Вектор %d: %.6f\n", i, data->results[i]);
    }
    fclose(file);
}

// Последовательная версия для сравнения
double sequential_version() {
    SharedData data;
    double start_time, end_time;
    
    // Выделение памяти
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    start_time = omp_get_wtime();
    
    // Последовательно выполняем все задачи
    task_read_vectors(&data);
    task_compute_products(&data); 
    task_save_results(&data);
    
    end_time = omp_get_wtime();
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
    
    return end_time - start_time;
}

// Параллельная версия с sections
double parallel_sections_version(int num_threads) {
    SharedData data;
    double start_time, end_time;
    
    // Инициализация флагов
    vectors_loaded = 0;
    computations_done = 0;
    
    // Выделение памяти
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    start_time = omp_get_wtime();
    
    // РАЗДЕЛЕНИЕ НА ТРИ ЗАДАЧИ С ПОМОЩЬЮ SECTIONS
    #pragma omp parallel sections num_threads(num_threads)
    {
        #pragma omp section
        {
            task_read_vectors(&data);
        }
        
        #pragma omp section
        {
            task_compute_products(&data);
        }
        
        #pragma omp section
        {
            task_save_results(&data);
        }
    }
    
    end_time = omp_get_wtime();
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
    
    return end_time - start_time;
}

// Эксперимент 1: Зависимость от количества потоков
void experiment_threads() {
    printf("\n🚀 ЭКСПЕРИМЕНТ 1: Зависимость от количества потоков\n");
    printf("================================================\n");
    printf("Параметры: %d векторов по %d элементов\n\n", NUM_VECTORS, VECTOR_SIZE);
    
    int thread_counts[] = {1, 2, 3, 4, 6, 8};
    int num_experiments = 6;
    
    printf("📊 РЕЗУЛЬТАТЫ:\n");
    printf("==============\n\n");
    
    // Заголовок таблицы
    printf("Threads | Sequential | Parallel | Speedup | Efficiency\n");
    printf("--------|------------|----------|---------|-----------\n");
    
    // Сначала получаем время последовательной версии
    double seq_time = sequential_version();
    
    // Тестируем параллельные версии
    for (int i = 0; i < num_experiments; i++) {
        int threads = thread_counts[i];
        double par_time = parallel_sections_version(threads);
        double speedup = seq_time / par_time;
        double efficiency = speedup / threads * 100;
        
        printf("%7d | %10.4f | %8.4f | %7.2fx | %6.1f%%\n", 
               threads, seq_time, par_time, speedup, efficiency);
    }
}

// Эксперимент 2: Зависимость от размера задачи
void experiment_sizes() {
    printf("\n🚀 ЭКСПЕРИМЕНТ 2: Зависимость от размера векторов\n");
    printf("================================================\n");
    printf("Параметры: %d векторов, %d потоков\n\n", NUM_VECTORS, NUM_THREADS);
    
    int sizes[] = {100000, 500000, 1000000, 2000000};
    int num_experiments = 4;
    
    printf("📊 РЕЗУЛЬТАТЫ:\n");
    printf("==============\n\n");
    
    // Заголовок таблицы
    printf("Size    | Sequential | Parallel | Speedup | Efficiency\n");
    printf("--------|------------|----------|---------|-----------\n");
    
    for (int i = 0; i < num_experiments; i++) {
        // Сохраняем оригинальный размер
        int original_size = VECTOR_SIZE;
        VECTOR_SIZE = sizes[i];
        
        // Генерируем данные нового размера
        generate_test_data();
        
        // Замеряем время
        double seq_time = sequential_version();
        double par_time = parallel_sections_version(NUM_THREADS);
        double speedup = seq_time / par_time;
        double efficiency = speedup / NUM_THREADS * 100;
        
        printf("%7d | %10.4f | %8.4f | %7.2fx | %6.1f%%\n", 
               sizes[i], seq_time, par_time, speedup, efficiency);
        
        // Восстанавливаем оригинальный размер
        VECTOR_SIZE = original_size;
    }
}

// Эксперимент 3: Сравнение времени выполнения отдельных задач
void experiment_tasks() {
    printf("\n🚀 ЭКСПЕРИМЕНТ 3: Время выполнения отдельных задач\n");
    printf("==================================================\n");
    printf("Параметры: %d векторов по %d элементов, %d потоков\n\n", 
           NUM_VECTORS, VECTOR_SIZE, NUM_THREADS);
    
    // Генерируем тестовые данные
    generate_test_data();
    
    SharedData data;
    double start_time, end_time;
    
    // Выделение памяти
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    printf("📊 ВРЕМЯ ВЫПОЛНЕНИЯ ЗАДАЧ:\n");
    printf("==========================\n\n");
    
    // Задача 1: Чтение данных
    vectors_loaded = 0;
    start_time = omp_get_wtime();
    task_read_vectors(&data);
    end_time = omp_get_wtime();
    double read_time = end_time - start_time;
    printf("Задача 1 (чтение): %.4f сек\n", read_time);
    
    // Задача 2: Вычисления
    computations_done = 0;
    start_time = omp_get_wtime();
    task_compute_products(&data);
    end_time = omp_get_wtime();
    double compute_time = end_time - start_time;
    printf("Задача 2 (вычисления): %.4f сек\n", compute_time);
    
    // Задача 3: Сохранение
    start_time = omp_get_wtime();
    task_save_results(&data);
    end_time = omp_get_wtime();
    double save_time = end_time - start_time;
    printf("Задача 3 (сохранение): %.4f сек\n", save_time);
    
    // Общее время
    double total_time = read_time + compute_time + save_time;
    printf("Общее время (сумма): %.4f сек\n", total_time);
    
    // Процентное соотношение
    printf("\nРАСПРЕДЕЛЕНИЕ ВРЕМЕНИ:\n");
    printf("Загрузка: %.1f%%, Вычисления: %.1f%%, Сохранение: %.1f%%\n",
           read_time/total_time*100, compute_time/total_time*100, save_time/total_time*100);
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
}

int main(int argc, char *argv[]) {
    printf("🔬 АВТОМАТИЧЕСКОЕ ТЕСТИРОВАНИЕ РАЗДЕЛЕНИЯ ЗАДАЧ\n");
    printf("===============================================\n");
    
    // Инициализация генератора случайных чисел
    srand(time(NULL));
    
    // Генерируем основные тестовые данные
    generate_test_data();
    
    // Запускаем все эксперименты
    experiment_threads();    // Зависимость от потоков
    experiment_sizes();      // Зависимость от размера
    experiment_tasks();      // Анализ времени задач
    
    printf("\n✅ Все эксперименты завершены!\n");
    printf("Данные готовы для построения графиков и анализа.\n");
    
    return 0;
}
