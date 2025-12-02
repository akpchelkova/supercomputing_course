#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// параметры которые будем менять в экспериментах
int NUM_VECTORS = 8;
int VECTOR_SIZE = 1000000;
int NUM_THREADS = 3;

// глобальные флаги для синхронизации между секциями
int vectors_loaded = 0;
int computations_done = 0;

// структура для хранения общих данных между задачами
typedef struct {
    double *vectors_a;  // массив для хранения первых векторов
    double *vectors_b;  // массив для хранения вторых векторов  
    double *results;    // массив для хранения результатов скалярных произведений
} SharedData;

// генерация тестовых данных в файлы
void generate_test_data() {
    FILE *file_a, *file_b;
    
    printf("генерация тестовых данных...\n");
    
    // создаем файл с векторами A
    file_a = fopen("vectors_a.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;  // случайные числа 0-10
            fwrite(&value, sizeof(double), 1, file_a);
        }
    }
    fclose(file_a);
    
    // создаем файл с векторами B  
    file_b = fopen("vectors_b.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;  // случайные числа 0-10
            fwrite(&value, sizeof(double), 1, file_b);
        }
    }
    fclose(file_b);
    
    printf("данные сгенерированы: %d векторов по %d элементов\n", NUM_VECTORS, VECTOR_SIZE);
}

// задача 1: чтение векторов из файла (выполняется в отдельной секции)
void task_read_vectors(SharedData *data) {
    FILE *file;
    
    // чтение vectors_a.dat
    file = fopen("vectors_a.dat", "rb");
    if (!file) {
        printf("ошибка: не могу открыть vectors_a.dat\n");
        return;
    }
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&data->vectors_a[i * VECTOR_SIZE + j], sizeof(double), 1, file);
        }
    }
    fclose(file);
    
    // чтение vectors_b.dat
    file = fopen("vectors_b.dat", "rb");
    if (!file) {
        printf("ошибка: не могу открыть vectors_b.dat\n");
        return;
    }
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&data->vectors_b[i * VECTOR_SIZE + j], sizeof(double), 1, file);
        }
    }
    fclose(file);
    
    // устанавливаем флаг загрузки данных для синхронизации с другими задачами
    #pragma omp atomic write
    vectors_loaded = 1;
    #pragma omp flush(vectors_loaded)  // принудительная синхронизация памяти между потоками
}

// функция вычисления скалярного произведения для одного вектора
double dot_product(double *a, double *b, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += a[i] * b[i];  // накапливаем сумму произведений элементов
    }
    return sum;
}

// задача 2: вычисление скалярных произведений (выполняется в отдельной секции)
void task_compute_products(SharedData *data) {
    // ожидаем загрузки данных из файлов перед началом вычислений
    while (1) {
        #pragma omp flush(vectors_loaded)  // обновляем значение флага из памяти
        if (vectors_loaded) break;  // выходим из цикла когда данные загружены
        for (int i = 0; i < 1000; i++) {} // короткая пауза чтобы не нагружать cpu
    }
    
    // вычисление скалярных произведений для всех пар векторов
    for (int i = 0; i < NUM_VECTORS; i++) {
        double *vec_a = &data->vectors_a[i * VECTOR_SIZE];  // указатель на i-й вектор A
        double *vec_b = &data->vectors_b[i * VECTOR_SIZE];  // указатель на i-й вектор B
        data->results[i] = dot_product(vec_a, vec_b, VECTOR_SIZE);  // вычисляем скалярное произведение
    }
    
    // устанавливаем флаг завершения вычислений для синхронизации
    #pragma omp atomic write
    computations_done = 1;
    #pragma omp flush(computations_done)  // принудительная синхронизация памяти
}

// задача 3: сохранение результатов в файл (выполняется в отдельной секции)
void task_save_results(SharedData *data) {
    // ожидаем завершения вычислений перед сохранением результатов
    while (1) {
        #pragma omp flush(computations_done)  // обновляем значение флага из памяти
        if (computations_done) break;  // выходим из цикла когда вычисления завершены
        for (int i = 0; i < 1000; i++) {} // короткая пауза
    }
    
    FILE *file = fopen("results.dat", "w");
    if (!file) {
        printf("ошибка: не могу создать results.dat\n");
        return;
    }
    
    // записываем результаты в файл
    for (int i = 0; i < NUM_VECTORS; i++) {
        fprintf(file, "вектор %d: %.6f\n", i, data->results[i]);
    }
    fclose(file);
}

// последовательная версия для сравнения производительности
double sequential_version() {
    SharedData data;
    double start_time, end_time;
    
    // выделение памяти для данных
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    start_time = omp_get_wtime();  // засекаем время начала
    
    // последовательно выполняем все задачи одна за другой
    task_read_vectors(&data);
    task_compute_products(&data); 
    task_save_results(&data);
    
    end_time = omp_get_wtime();  // засекаем время окончания
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
    
    return end_time - start_time;  // возвращаем общее время выполнения
}

// параллельная версия с использованием sections
double parallel_sections_version(int num_threads) {
    SharedData data;
    double start_time, end_time;
    
    // инициализация флагов синхронизации
    vectors_loaded = 0;
    computations_done = 0;
    
    // выделение памяти для данных
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    start_time = omp_get_wtime();  // засекаем время начала
    
    // разделение на три независимые задачи с помощью директивы sections
    #pragma omp parallel sections num_threads(num_threads)
    {
        #pragma omp section
        {
            // задача 1: чтение данных из файлов (выполняется в одном потоке)
            task_read_vectors(&data);
        }
        
        #pragma omp section
        {
            // задача 2: вычисления (выполняется в другом потоке)
            task_compute_products(&data);
        }
        
        #pragma omp section
        {
            // задача 3: сохранение результатов (выполняется в третьем потоке)
            task_save_results(&data);
        }
    }
    
    end_time = omp_get_wtime();  // засекаем время окончания
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
    
    return end_time - start_time;  // возвращаем общее время выполнения
}

// эксперимент 1: исследование зависимости производительности от количества потоков
void experiment_threads() {
    printf("\nэксперимент 1: зависимость от количества потоков\n");
    printf("================================================\n");
    printf("параметры: %d векторов по %d элементов\n\n", NUM_VECTORS, VECTOR_SIZE);
    
    int thread_counts[] = {1, 2, 3, 4, 6, 8};  // тестируемые количества потоков
    int num_experiments = 6;
    
    printf("результаты:\n");
    printf("==============\n\n");
    
    // заголовок таблицы для вывода результатов
    printf("threads | sequential | parallel | speedup | efficiency\n");
    printf("--------|------------|----------|---------|-----------\n");
    
    // сначала получаем время последовательной версии для сравнения
    double seq_time = sequential_version();
    
    // тестируем параллельные версии с разным количеством потоков
    for (int i = 0; i < num_experiments; i++) {
        int threads = thread_counts[i];
        double par_time = parallel_sections_version(threads);
        double speedup = seq_time / par_time;  // вычисляем ускорение
        double efficiency = speedup / threads * 100;  // вычисляем эффективность
        
        printf("%7d | %10.4f | %8.4f | %7.2fx | %6.1f%%\n", 
               threads, seq_time, par_time, speedup, efficiency);
    }
}

// эксперимент 2: исследование зависимости от размера векторов
void experiment_sizes() {
    printf("\nэксперимент 2: зависимость от размера векторов\n");
    printf("================================================\n");
    printf("параметры: %d векторов, %d потоков\n\n", NUM_VECTORS, NUM_THREADS);
    
    int sizes[] = {100000, 500000, 1000000, 2000000};  // тестируемые размеры векторов
    int num_experiments = 4;
    
    printf("результаты:\n");
    printf("==============\n\n");
    
    // заголовок таблицы для вывода результатов
    printf("size    | sequential | parallel | speedup | efficiency\n");
    printf("--------|------------|----------|---------|-----------\n");
    
    for (int i = 0; i < num_experiments; i++) {
        // сохраняем оригинальный размер
        int original_size = VECTOR_SIZE;
        VECTOR_SIZE = sizes[i];  // устанавливаем новый размер для эксперимента
        
        // генерируем данные нового размера
        generate_test_data();
        
        // замеряем время выполнения для нового размера
        double seq_time = sequential_version();
        double par_time = parallel_sections_version(NUM_THREADS);
        double speedup = seq_time / par_time;
        double efficiency = speedup / NUM_THREADS * 100;
        
        printf("%7d | %10.4f | %8.4f | %7.2fx | %6.1f%%\n", 
               sizes[i], seq_time, par_time, speedup, efficiency);
        
        // восстанавливаем оригинальный размер
        VECTOR_SIZE = original_size;
    }
}

// эксперимент 3: анализ времени выполнения отдельных задач
void experiment_tasks() {
    printf("\nэксперимент 3: время выполнения отдельных задач\n");
    printf("==================================================\n");
    printf("параметры: %d векторов по %d элементов, %d потоков\n\n", 
           NUM_VECTORS, VECTOR_SIZE, NUM_THREADS);
    
    // генерируем тестовые данные
    generate_test_data();
    
    SharedData data;
    double start_time, end_time;
    
    // выделение памяти для данных
    data.vectors_a = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.vectors_b = (double*)malloc(NUM_VECTORS * VECTOR_SIZE * sizeof(double));
    data.results = (double*)malloc(NUM_VECTORS * sizeof(double));
    
    printf("время выполнения задач:\n");
    printf("==========================\n\n");
    
    // задача 1: чтение данных
    vectors_loaded = 0;
    start_time = omp_get_wtime();
    task_read_vectors(&data);
    end_time = omp_get_wtime();
    double read_time = end_time - start_time;
    printf("задача 1 (чтение): %.4f сек\n", read_time);
    
    // задача 2: вычисления
    computations_done = 0;
    start_time = omp_get_wtime();
    task_compute_products(&data);
    end_time = omp_get_wtime();
    double compute_time = end_time - start_time;
    printf("задача 2 (вычисления): %.4f сек\n", compute_time);
    
    // задача 3: сохранение
    start_time = omp_get_wtime();
    task_save_results(&data);
    end_time = omp_get_wtime();
    double save_time = end_time - start_time;
    printf("задача 3 (сохранение): %.4f сек\n", save_time);
    
    // общее время выполнения всех задач
    double total_time = read_time + compute_time + save_time;
    printf("общее время (сумма): %.4f сек\n", total_time);
    
    // процентное соотношение времени выполнения задач
    printf("\nраспределение времени:\n");
    printf("загрузка: %.1f%%, вычисления: %.1f%%, сохранение: %.1f%%\n",
           read_time/total_time*100, compute_time/total_time*100, save_time/total_time*100);
    
    free(data.vectors_a);
    free(data.vectors_b);
    free(data.results);
}

int main(int argc, char *argv[]) {
    printf("автоматическое тестирование разделения задач\n");
    printf("===============================================\n");
    
    // инициализация генератора случайных чисел
    srand(time(NULL));
    
    // генерируем основные тестовые данные
    generate_test_data();
    
    // запускаем все эксперименты
    experiment_threads();    // зависимость от количества потоков
    experiment_sizes();      // зависимость от размера векторов
    experiment_tasks();      // анализ времени выполнения отдельных задач
    
    printf("\nвсе эксперименты завершены!\n");
    printf("данные готовы для построения графиков и анализа.\n");
    
    return 0;
}
