#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define MATRIX_SIZE 2000

// Заполнение матрицы случайными числами
void fill_matrix(double **matrix, int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
        }
    }
}

// 1. ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ (базовая)
double sequential_version(double **matrix, int size) {
    double max_of_min = -1.0;
    
    for (int i = 0; i < size; i++) {
        double row_min = matrix[i][0];
        for (int j = 1; j < size; j++) {
            if (matrix[i][j] < row_min) {
                row_min = matrix[i][j];
            }
        }
        if (row_min > max_of_min) {
            max_of_min = row_min;
        }
    }
    
    return max_of_min;
}

// 2. ТОЛЬКО ВНЕШНИЙ ПАРАЛЛЕЛИЗМ
double outer_parallel_only(double **matrix, int size) {
    double max_of_min = -1.0;
    
    #pragma omp parallel
    {
        double local_max = -1.0;
        
        #pragma omp for
        for (int i = 0; i < size; i++) {
            double row_min = matrix[i][0];
            for (int j = 1; j < size; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            if (row_min > local_max) {
                local_max = row_min;
            }
        }
        
        #pragma omp critical
        {
            if (local_max > max_of_min) {
                max_of_min = local_max;
            }
        }
    }
    
    return max_of_min;
}

// 3. ВЛОЖЕННЫЙ ПАРАЛЛЕЛИЗМ (оба цикла параллельны)
double nested_parallel_both(double **matrix, int size) {
    double max_of_min = -1.0;
    
    #pragma omp parallel
    {
        double local_max = -1.0;
        
        #pragma omp for
        for (int i = 0; i < size; i++) {
            double row_min = matrix[i][0];
            
            // ВЛОЖЕННЫЙ ПАРАЛЛЕЛИЗМ - внутренний цикл
            #pragma omp parallel for reduction(min:row_min)
            for (int j = 0; j < size; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            
            if (row_min > local_max) {
                local_max = row_min;
            }
        }
        
        #pragma omp critical
        {
            if (local_max > max_of_min) {
                max_of_min = local_max;
            }
        }
    }
    
    return max_of_min;
}

// 4. ВЛОЖЕННЫЙ ПАРАЛЛЕЛИЗМ С ОГРАНИЧЕНИЕМ ПОТОКОВ
double nested_parallel_controlled(double **matrix, int size) {
    double max_of_min = -1.0;
    
    #pragma omp parallel
    {
        double local_max = -1.0;
        
        #pragma omp for
        for (int i = 0; i < size; i++) {
            double row_min = matrix[i][0];
            
            // Вложенный параллелизм с ограничением потоков
            #pragma omp parallel for reduction(min:row_min) num_threads(2)
            for (int j = 0; j < size; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            
            if (row_min > local_max) {
                local_max = row_min;
            }
        }
        
        #pragma omp critical
        {
            if (local_max > max_of_min) {
                max_of_min = local_max;
            }
        }
    }
    
    return max_of_min;
}

int main() {
    int size = MATRIX_SIZE;
    double **matrix;
    double start_time, end_time;
    
    // Выделение памяти
    matrix = (double**)malloc(size * sizeof(double*));
    for (int i = 0; i < size; i++) {
        matrix[i] = (double*)malloc(size * sizeof(double));
    }
    
    // Заполнение матрицы
    srand(time(NULL));
    fill_matrix(matrix, size);
    
    printf("СРАВНЕНИЕ СТРАТЕГИЙ ПАРАЛЛЕЛИЗМА ДЛЯ ЗАДАЧИ 4\n");
    printf("=============================================\n");
    printf("Задача: максимум среди минимумов строк матрицы\n");
    printf("Размер матрицы: %d x %d\n", size, size);
    printf("Количество потоков (внешних): 4\n\n");
    
    // Включаем вложенный параллелизм
    omp_set_nested(1);
    omp_set_num_threads(4);
    
    double result;
    
    // ТЕСТ 1: Последовательная версия
    printf("1. ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ:\n");
    start_time = omp_get_wtime();
    result = sequential_version(matrix, size);
    end_time = omp_get_wtime();
    printf("   Результат: %.2f\n", result);
    printf("   Время: %.4f сек\n\n", end_time - start_time);
    double seq_time = end_time - start_time;
    
    // ТЕСТ 2: Только внешний параллелизм
    printf("2. ТОЛЬКО ВНЕШНИЙ ПАРАЛЛЕЛИЗМ:\n");
    start_time = omp_get_wtime();
    result = outer_parallel_only(matrix, size);
    end_time = omp_get_wtime();
    printf("   Результат: %.2f\n", result);
    printf("   Время: %.4f сек\n", end_time - start_time);
    printf("   Ускорение: %.2fx\n\n", seq_time / (end_time - start_time));
    
    // ТЕСТ 3: Вложенный параллелизм (оба цикла)
    printf("3. ВЛОЖЕННЫЙ ПАРАЛЛЕЛИЗМ (оба цикла):\n");
    start_time = omp_get_wtime();
    result = nested_parallel_both(matrix, size);
    end_time = omp_get_wtime();
    printf("   Результат: %.2f\n", result);
    printf("   Время: %.4f сек\n", end_time - start_time);
    printf("   Ускорение: %.2fx\n\n", seq_time / (end_time - start_time));
    
    // ТЕСТ 4: Вложенный параллелизм с контролем
    printf("4. ВЛОЖЕННЫЙ ПАРАЛЛЕЛИЗМ (контролируемый):\n");
    start_time = omp_get_wtime();
    result = nested_parallel_controlled(matrix, size);
    end_time = omp_get_wtime();
    printf("   Результат: %.2f\n", result);
    printf("   Время: %.4f сек\n", end_time - start_time);
    printf("   Ускорение: %.2fx\n\n", seq_time / (end_time - start_time));
    
    // Освобождение памяти
    for (int i = 0; i < size; i++) {
        free(matrix[i]);
    }
    free(matrix);
    
    return 0;
}
