#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// параметры которые будем менять в экспериментах
int NUM_VECTORS = 8;
int VECTOR_SIZE = 1000000;
int NUM_THREADS = 4;  // увеличиваем для лучшего конвейера

// структура для хранения пары векторов и их состояния
typedef struct {
    double *vector_a;    // данные первого вектора
    double *vector_b;    // данные второго вектора  
    int a_loaded;        // флаг загрузки вектора a (0/1)
    int b_loaded;        // флаг загрузки вектора b (0/1)
    int computed;        // флаг вычислений (0/1)
    double result;       // результат скалярного произведения
} VectorPair;

// глобальный массив пар векторов для конвейерной обработки
VectorPair *vector_pairs = NULL;

// функция для генерации тестовых данных в файлы
void generate_test_data() {
    FILE *file_a, *file_b;
    
    printf("генерация тестовых данных...\n");
    
    // создаем файл с векторами A
    file_a = fopen("vectors_a.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_a);
        }
    }
    fclose(file_a);
    
    // создаем файл с векторами B  
    file_b = fopen("vectors_b.dat", "wb");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_b);
        }
    }
    fclose(file_b);
    
    printf("данные сгенерированы: %d векторов по %d элементов\n", NUM_VECTORS, VECTOR_SIZE);
}

// функция вычисления скалярного произведения для одного вектора
double dot_product(double *a, double *b, int size) {
    double sum = 0.0;
    // используем simd для векторизации вычислений внутри скалярного произведения
    #pragma omp simd reduction(+:sum)
    for (int i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// задача чтения одного вектора A из файла
void read_vector_a(int pair_index) {
    FILE *file = fopen("vectors_a.dat", "rb");
    if (!file) {
        printf("ошибка: не могу открыть vectors_a.dat\n");
        return;
    }
    
    // перемещаемся к позиции нужного вектора в файле
    long offset = pair_index * VECTOR_SIZE * sizeof(double);
    fseek(file, offset, SEEK_SET);
    
    // читаем данные вектора A
    for (int j = 0; j < VECTOR_SIZE; j++) {
        fread(&vector_pairs[pair_index].vector_a[j], sizeof(double), 1, file);
    }
    
    fclose(file);
    
    // атомарно устанавливаем флаг что вектор A загружен
    #pragma omp atomic write
    vector_pairs[pair_index].a_loaded = 1;
}

// задача чтения одного вектора B из файла
void read_vector_b(int pair_index) {
    FILE *file = fopen("vectors_b.dat", "rb");
    if (!file) {
        printf("ошибка: не могу открыть vectors_b.dat\n");
        return;
    }
    
    // перемещаемся к позиции нужного вектора в файле
    long offset = pair_index * VECTOR_SIZE * sizeof(double);
    fseek(file, offset, SEEK_SET);
    
    // читаем данные вектора B
    for (int j = 0; j < VECTOR_SIZE; j++) {
        fread(&vector_pairs[pair_index].vector_b[j], sizeof(double), 1, file);
    }
    
    fclose(file);
    
    // атомарно устанавливаем флаг что вектор B загружен
    #pragma omp atomic write
    vector_pairs[pair_index].b_loaded = 1;
}

// задача вычисления скалярного произведения для одной пары векторов
void compute_vector_pair(int pair_index) {
    // ожидаем пока оба вектора будут загружены
    while (1) {
        int a_loaded, b_loaded;
        // атомарно читаем флаги загрузки
        #pragma omp atomic read
        a_loaded = vector_pairs[pair_index].a_loaded;
        #pragma omp atomic read
        b_loaded = vector_pairs[pair_index].b_loaded;
        
        if (a_loaded && b_loaded) break;  // выходим когда оба загружены
        
        // короткая активная пауза чтобы не нагружать cpu
        for (int i = 0; i < 100; i++) {}
    }
    
    // вычисляем скалярное произведение
    vector_pairs[pair_index].result = dot_product(vector_pairs[pair_index].vector_a, 
                                                  vector_pairs[pair_index].vector_b, 
                                                  VECTOR_SIZE);
    
    // атомарно устанавливаем флаг что вычисления завершены
    #pragma omp atomic write
    vector_pairs[pair_index].computed = 1;
}

// задача сохранения одного результата в файл
void save_single_result(int pair_index, FILE *file) {
    // ожидаем пока вычисления для этой пары завершатся
    while (1) {
        int computed;
        #pragma omp atomic read
        computed = vector_pairs[pair_index].computed;
        if (computed) break;
        
        // короткая активная пауза
        for (int i = 0; i < 100; i++) {}
    }
    
    // используем критическую секцию для безопасной записи в файл
    #pragma omp critical
    {
        fprintf(file, "вектор %d: %.6f\n", pair_index, vector_pairs[pair_index].result);
    }
}

// конвейерная версия с использованием задач openmp
double pipeline_tasks_version(int num_threads) {
    double start_time, end_time;
    int error_flag = 0;  // флаг ошибки
    
    // выделение памяти для всех пар векторов
    vector_pairs = (VectorPair*)malloc(NUM_VECTORS * sizeof(VectorPair));
    if (vector_pairs == NULL) {
        printf("ошибка выделения памяти\n");
        return 0.0;
    }
    
    for (int i = 0; i < NUM_VECTORS; i++) {
        vector_pairs[i].vector_a = (double*)malloc(VECTOR_SIZE * sizeof(double));
        vector_pairs[i].vector_b = (double*)malloc(VECTOR_SIZE * sizeof(double));
        
        if (vector_pairs[i].vector_a == NULL || vector_pairs[i].vector_b == NULL) {
            printf("ошибка выделения памяти для вектора %d\n", i);
            error_flag = 1;
            break;
        }
        
        vector_pairs[i].a_loaded = 0;
        vector_pairs[i].b_loaded = 0;
        vector_pairs[i].computed = 0;
        vector_pairs[i].result = 0.0;
    }
    
    if (error_flag) {
        // освобождаем уже выделенную память
        for (int i = 0; i < NUM_VECTORS; i++) {
            if (vector_pairs[i].vector_a) free(vector_pairs[i].vector_a);
            if (vector_pairs[i].vector_b) free(vector_pairs[i].vector_b);
        }
        free(vector_pairs);
        return 0.0;
    }
    
    start_time = omp_get_wtime();
    
    // создаем параллельную область с указанным количеством потоков
    #pragma omp parallel num_threads(num_threads)
    {
        // только один поток будет создавать задачи
        #pragma omp single
        {
            // открываем файл для записи результатов
            FILE *results_file = fopen("results_pipeline.dat", "w");
            if (!results_file) {
                printf("ошибка создания файла результатов\n");
                // устанавливаем флаг ошибки, но не выходим из блока
                error_flag = 1;
            } else {
                // создаем задачи для конвейерной обработки каждой пары векторов
                for (int i = 0; i < NUM_VECTORS; i++) {
                    // задача на чтение вектора A
                    #pragma omp task firstprivate(i)
                    {
                        read_vector_a(i);
                    }
                    
                    // задача на чтение вектора B (может выполняться параллельно с чтением A)
                    #pragma omp task firstprivate(i)
                    {
                        read_vector_b(i);
                    }
                    
                    // задача на вычисления (зависит от завершения чтения обоих векторов)
                    #pragma omp task firstprivate(i)
                    {
                        compute_vector_pair(i);
                    }
                    
                    // задача на сохранение (зависит от завершения вычислений)
                    #pragma omp task firstprivate(i)
                    {
                        save_single_result(i, results_file);
                    }
                }
                
                // ждем завершения всех созданных задач
                #pragma omp taskwait
                
                fclose(results_file);
            }
        }
    }
    
    end_time = omp_get_wtime();
    
    // освобождение памяти
    for (int i = 0; i < NUM_VECTORS; i++) {
        free(vector_pairs[i].vector_a);
        free(vector_pairs[i].vector_b);
    }
    free(vector_pairs);
    
    if (error_flag) {
        return 0.0;
    }
    
    return end_time - start_time;
}

// экспериментальная версия с циклическим конвейером на трех потоках
double circular_pipeline_version() {
    double start_time, end_time;
    
    // глубина конвейера - количество одновременно обрабатываемых пар
    #define PIPELINE_DEPTH 3
    
    // создаем буфер для конвейерной обработки
    VectorPair pipeline[PIPELINE_DEPTH];
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        pipeline[i].vector_a = (double*)malloc(VECTOR_SIZE * sizeof(double));
        pipeline[i].vector_b = (double*)malloc(VECTOR_SIZE * sizeof(double));
        pipeline[i].a_loaded = 0;
        pipeline[i].b_loaded = 0;
        pipeline[i].computed = 0;
    }
    
    FILE *results_file = fopen("results_circular.dat", "w");
    
    start_time = omp_get_wtime();
    
    // создаем три независимых потока для конвейера
    #pragma omp parallel sections num_threads(3)
    {
        // поток 1: чтение данных
        #pragma omp section
        {
            for (int i = 0; i < NUM_VECTORS; i++) {
                int buffer_index = i % PIPELINE_DEPTH;  // циклическое использование буферов
                
                // ждем пока буфер освободится от предыдущей обработки
                while (pipeline[buffer_index].a_loaded || pipeline[buffer_index].b_loaded) {
                    for (int j = 0; j < 100; j++) {}  // активное ожидание
                }
                
                // читаем вектор A
                FILE *file_a = fopen("vectors_a.dat", "rb");
                long offset = i * VECTOR_SIZE * sizeof(double);
                fseek(file_a, offset, SEEK_SET);
                for (int j = 0; j < VECTOR_SIZE; j++) {
                    fread(&pipeline[buffer_index].vector_a[j], sizeof(double), 1, file_a);
                }
                fclose(file_a);
                pipeline[buffer_index].a_loaded = 1;
                
                // читаем вектор B (может начаться сразу после начала чтения A)
                FILE *file_b = fopen("vectors_b.dat", "rb");
                fseek(file_b, offset, SEEK_SET);
                for (int j = 0; j < VECTOR_SIZE; j++) {
                    fread(&pipeline[buffer_index].vector_b[j], sizeof(double), 1, file_b);
                }
                fclose(file_b);
                pipeline[buffer_index].b_loaded = 1;
            }
        }
        
        // поток 2: вычисления
        #pragma omp section
        {
            for (int i = 0; i < NUM_VECTORS; i++) {
                int buffer_index = i % PIPELINE_DEPTH;
                
                // ждем пока оба вектора будут загружены
                while (!pipeline[buffer_index].a_loaded || !pipeline[buffer_index].b_loaded) {
                    for (int j = 0; j < 100; j++) {}
                }
                
                // вычисляем скалярное произведение
                pipeline[buffer_index].result = dot_product(pipeline[buffer_index].vector_a,
                                                          pipeline[buffer_index].vector_b,
                                                          VECTOR_SIZE);
                pipeline[buffer_index].computed = 1;
            }
        }
        
        // поток 3: сохранение результатов
        #pragma omp section
        {
            for (int i = 0; i < NUM_VECTORS; i++) {
                int buffer_index = i % PIPELINE_DEPTH;
                
                // ждем пока вычисления завершатся
                while (!pipeline[buffer_index].computed) {
                    for (int j = 0; j < 100; j++) {}
                }
                
                // сохраняем результат
                fprintf(results_file, "вектор %d: %.6f\n", i, pipeline[buffer_index].result);
                
                // сбрасываем флаги для повторного использования буфера
                pipeline[buffer_index].a_loaded = 0;
                pipeline[buffer_index].b_loaded = 0;
                pipeline[buffer_index].computed = 0;
            }
        }
    }
    
    end_time = omp_get_wtime();
    
    fclose(results_file);
    
    // освобождение памяти буфера
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        free(pipeline[i].vector_a);
        free(pipeline[i].vector_b);
    }
    
    return end_time - start_time;
}

// последовательная версия для сравнения производительности
double sequential_version() {
    double start_time, end_time;
    
    // выделение памяти
    VectorPair local_pairs[NUM_VECTORS];
    for (int i = 0; i < NUM_VECTORS; i++) {
        local_pairs[i].vector_a = (double*)malloc(VECTOR_SIZE * sizeof(double));
        local_pairs[i].vector_b = (double*)malloc(VECTOR_SIZE * sizeof(double));
    }
    
    start_time = omp_get_wtime();
    
    FILE *results_file = fopen("results_sequential.dat", "w");
    
    // последовательная обработка каждой пары
    for (int i = 0; i < NUM_VECTORS; i++) {
        // чтение вектора A
        FILE *file_a = fopen("vectors_a.dat", "rb");
        long offset = i * VECTOR_SIZE * sizeof(double);
        fseek(file_a, offset, SEEK_SET);
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&local_pairs[i].vector_a[j], sizeof(double), 1, file_a);
        }
        fclose(file_a);
        
        // чтение вектора B
        FILE *file_b = fopen("vectors_b.dat", "rb");
        fseek(file_b, offset, SEEK_SET);
        for (int j = 0; j < VECTOR_SIZE; j++) {
            fread(&local_pairs[i].vector_b[j], sizeof(double), 1, file_b);
        }
        fclose(file_b);
        
        // вычисления
        local_pairs[i].result = dot_product(local_pairs[i].vector_a, local_pairs[i].vector_b, VECTOR_SIZE);
        
        // сохранение
        fprintf(results_file, "вектор %d: %.6f\n", i, local_pairs[i].result);
    }
    
    fclose(results_file);
    
    end_time = omp_get_wtime();
    
    // освобождение памяти
    for (int i = 0; i < NUM_VECTORS; i++) {
        free(local_pairs[i].vector_a);
        free(local_pairs[i].vector_b);
    }
    
    return end_time - start_time;
}

// эксперимент: сравнение разных версий
void run_comparison_experiment() {
    printf("\nсравнение конвейерной обработки\n");
    printf("===============================\n");
    
    // генерируем тестовые данные
    generate_test_data();
    
    printf("\nпараметры эксперимента:\n");
    printf("- количество векторов: %d\n", NUM_VECTORS);
    printf("- размер вектора: %d элементов\n", VECTOR_SIZE);
    printf("- объем данных: %.2f MB на файл\n", 
           (double)NUM_VECTORS * VECTOR_SIZE * sizeof(double) / (1024*1024));
    
    printf("\nзапуск тестов...\n\n");
    
    // запускаем разные версии и замеряем время
    double time_seq = sequential_version();
    double time_tasks = pipeline_tasks_version(4);
    double time_circular = circular_pipeline_version();
    
    printf("результаты:\n");
    printf("------------\n");
    printf("последовательная версия:          %.4f секунд\n", time_seq);
    printf("конвейерная версия (tasks):       %.4f секунд\n", time_tasks);
    printf("циклический конвейер (sections):  %.4f секунд\n", time_circular);
    
    printf("\nускорение:\n");
    printf("----------\n");
    printf("tasks vs последовательная:    %.2fx\n", time_seq / time_tasks);
    printf("circular vs последовательная: %.2fx\n", time_seq / time_circular);
    printf("circular vs tasks:            %.2fx\n", time_tasks / time_circular);
    
    printf("\nэффективность конвейера:\n");
    printf("-----------------------\n");
    printf("идеальное ускорение для 4 потоков: 4.00x\n");
    printf("достигнутое ускорение:             %.2fx\n", time_seq / time_tasks);
    printf("эффективность:                     %.1f%%\n", 
           (time_seq / time_tasks) / 4 * 100);
}

int main(int argc, char *argv[]) {
    printf("конвейерная обработка скалярных произведений векторов\n");
    printf("=====================================================\n");
    
    // инициализация генератора случайных чисел
    srand(time(NULL));
    
    // запускаем эксперимент сравнения
    run_comparison_experiment();
    
    printf("\nэксперимент завершен!\n");
    printf("файлы результатов:\n");
    printf("- results_sequential.dat: последовательная версия\n");
    printf("- results_pipeline.dat: конвейерная версия (tasks)\n");
    printf("- results_circular.dat: циклический конвейер (sections)\n");
    
    return 0;
}
