#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define NUM_VECTORS 8
#define VECTOR_SIZE 1000000

// Глобальные флаги для синхронизации (должны быть разделяемыми)
int vectors_loaded = 0;
int computations_done = 0;

// Структура для данных
typedef struct {
    double *vectors_a;
    double *vectors_b;
    double *results;
} SharedData;

// ЗАДАЧА 1: Чтение векторов из файла
void task_read_vectors(SharedData *data) {
    FILE *file;
    printf("ЗАДАЧА 1 (поток %d): Начало чтения векторов...\n", omp_get_thread_num());
    
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
    printf("ЗАДАЧА 1: vectors_a.dat загружен\n");
    
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
    printf("ЗАДАЧА 1: vectors_b.dat загружен\n");
    
    // Устанавливаем флаг и принудительно обновляем для всех потоков
    #pragma omp atomic write
    vectors_loaded = 1;
    #pragma omp flush(vectors_loaded)
    
    printf("ЗАДАЧА 1 (поток %d): Все данные загружены\n", omp_get_thread_num());
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
    printf("ЗАДАЧА 2 (поток %d): Ожидание данных...\n", omp_get_thread_num());
    
    // Ожидаем загрузки данных с периодической проверкой
    while (1) {
        #pragma omp flush(vectors_loaded)
        if (vectors_loaded) break;
        // Короткая пауза чтобы не грузить CPU
        for (int i = 0; i < 1000; i++) {}
    }
    
    printf("ЗАДАЧА 2 (поток %d): Начало вычислений\n", omp_get_thread_num());
    
    // Вычисление скалярных произведений
    for (int i = 0; i < NUM_VECTORS; i++) {
        double *vec_a = &data->vectors_a[i * VECTOR_SIZE];
        double *vec_b = &data->vectors_b[i * VECTOR_SIZE];
        data->results[i] = dot_product(vec_a, vec_b, VECTOR_SIZE);
        printf("  Вычислен вектор %d: %.2f\n", i, data->results[i]);
    }
    
    // Устанавливаем флаг завершения вычислений
    #pragma omp atomic write
    computations_done = 1;
    #pragma omp flush(computations_done)
    
    printf("ЗАДАЧА 2 (поток %d): Все вычисления завершены\n", omp_get_thread_num());
}

// ЗАДАЧА 3: Сохранение результатов
void task_save_results(SharedData *data) {
    printf("ЗАДАЧА 3 (поток %d): Ожидание результатов вычислений...\n", omp_get_thread_num());
    
    // Ожидаем завершения вычислений
    while (1) {
        #pragma omp flush(computations_done)
        if (computations_done) break;
        // Короткая пауза
        for (int i = 0; i < 1000; i++) {}
    }
    
    printf("ЗАДАЧА 3 (поток %d): Сохранение результатов...\n", omp_get_thread_num());
    
    FILE *file = fopen("results.dat", "w");
    if (!file) {
        printf("Ошибка: не могу создать results.dat\n");
        return;
    }
    
    for (int i = 0; i < NUM_VECTORS; i++) {
        fprintf(file, "Вектор %d: %.6f\n", i, data->results[i]);
    }
    fclose(file);
    
    printf("ЗАДАЧА 3 (поток %d): Результаты сохранены в results.dat\n", omp_get_thread_num());
}

int main() {
    SharedData data;
    double start_time, end_time;
    
    // Инициализация флагов
    vectors_loaded = 0;
    computations_done = 0;
    
    // Выделение памяти
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    printf("ВЫЧИСЛЕНИЕ СКАЛЯРНЫХ ПРОИЗВЕДЕНИЙ С РАЗДЕЛЕНИЕМ ЗАДАЧ\n");
    printf("=====================================================\n");
    printf("Количество векторов: %d\n", NUM_VECTORS);
    printf("Размер каждого вектора: %d\n", VECTOR_SIZE);
    printf("Файлы данных: vectors_a.dat, vectors_b.dat\n");
    printf("Файл результатов: results.dat\n\n");
    
    start_time = omp_get_wtime();
    
    // РАЗДЕЛЕНИЕ НА ТРИ ЗАДАЧИ С ПОМОЩЬЮ SECTIONS
    #pragma omp parallel sections num_threads(3)
    {
        #pragma omp section
        {
            // ЗАДАЧА 1: Ввод данных (I/O операция)
            task_read_vectors(&data);
        }
        
        #pragma omp section
        {
            // ЗАДАЧА 2: Вычисления (CPU операция)
            task_compute_products(&data);
        }
        
        #pragma omp section
        {
            // ЗАДАЧА 3: Сохранение результатов (I/O операция)
            task_save_results(&data);
        }
    }
    
    end_time = omp_get_wtime();
    
    printf("\nОБЩИЙ РЕЗУЛЬТАТ:\n");
    printf("================\n");
    printf("Все задачи завершены\n");
    printf("Общее время выполнения: %.4f секунд\n", end_time - start_time);
    printf("Результаты сохранены в файл: results.dat\n");
    
    // Вывод нескольких результатов на экран
    printf("\nПЕРВЫЕ 3 РЕЗУЛЬТАТА:\n");
    for (int i = 0; i < 3; i++) {
        printf("  Вектор %d: %.2f\n", i, data.results[i]);
    }
    
    // Освобождение памяти
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
    
    return 0;
}
