#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>

// Функция для заполнения матрицы случайными числами
void fill_matrix(double **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;  // числа от 0 до 100
        }
    }
}

// Функция для вывода матрицы (для маленьких размеров)
void print_matrix(double **matrix, int rows, int cols) {
    if (rows > 10 || cols > 10) {
        printf("Матрица слишком большая для вывода\n");
        return;
    }
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%6.1f ", matrix[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    // Размер матрицы можно передавать как аргументы
    int rows = 1000;    // строки по умолчанию
    int cols = 1000;    // столбцы по умолчанию
    
    if (argc > 2) {
        rows = atoi(argv[1]);
        cols = atoi(argv[2]);
    }

    // Выделяем память под матрицу
    double **matrix = (double**)malloc(rows * sizeof(double*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (double*)malloc(cols * sizeof(double));
    }

    // Инициализируем генератор случайных чисел
    srand(time(NULL));
    fill_matrix(matrix, rows, cols);

    printf("Размер матрицы: %d x %d\n", rows, cols);
    
    // Выводим матрицу только если она маленькая
    if (rows <= 5 && cols <= 5) {
        printf("Матрица:\n");
        print_matrix(matrix, rows, cols);
    }

    // ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ
    double seq_result = 0.0;
    double seq_start = omp_get_wtime();
    
    // Находим минимумы для каждой строки
    double *row_minima = (double*)malloc(rows * sizeof(double));
    for (int i = 0; i < rows; i++) {
        double min_val = matrix[i][0];
        for (int j = 1; j < cols; j++) {
            if (matrix[i][j] < min_val) {
                min_val = matrix[i][j];
            }
        }
        row_minima[i] = min_val;
    }
    
    // Находим максимум среди минимумов строк
    seq_result = row_minima[0];
    for (int i = 1; i < rows; i++) {
        if (row_minima[i] > seq_result) {
            seq_result = row_minima[i];
        }
    }
    
    double seq_time = omp_get_wtime() - seq_start;

    printf("\nПоследовательная версия:\n");
    printf("  Максимум среди минимумов строк: %.2f\n", seq_result);
    printf("  Время: %.4f секунд\n", seq_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ С РЕДУКЦИЕЙ
    double red_result = 0.0;
    double red_start = omp_get_wtime();

    #pragma omp parallel
    {
        double local_max = -1.0;  // локальный максимум для текущего потока
        
        #pragma omp for
        for (int i = 0; i < rows; i++) {
            // Находим минимум в текущей строке
            double row_min = matrix[i][0];
            for (int j = 1; j < cols; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            
            // Обновляем локальный максимум
            if (row_min > local_max) {
                local_max = row_min;
            }
        }
        
        // Редукция для нахождения глобального максимума
        #pragma omp critical
        {
            if (local_max > red_result) {
                red_result = local_max;
            }
        }
    }

    double red_time = omp_get_wtime() - red_start;

    printf("\nПараллельная версия (редукция):\n");
    printf("  Максимум среди минимумов строк: %.2f\n", red_result);
    printf("  Время: %.4f секунд\n", red_time);
    printf("  Ускорение: %.2fx\n", seq_time / red_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ БЕЗ РЕДУКЦИИ (вложенный параллелизм)
    double nested_result = 0.0;
    double nested_start = omp_get_wtime();

    #pragma omp parallel
    {
        double local_max = -1.0;
        
        // Распараллеливаем внешний цикл по строкам
        #pragma omp for
        for (int i = 0; i < rows; i++) {
            double row_min = matrix[i][0];
            
            // Внутренний цикл можно тоже распараллелить (вложенный параллелизм)
            #pragma omp parallel for reduction(min:row_min)
            for (int j = 0; j < cols; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            
            #pragma omp critical
            {
                if (row_min > local_max) {
                    local_max = row_min;
                }
            }
        }
        
        #pragma omp critical
        {
            if (local_max > nested_result) {
                nested_result = local_max;
            }
        }
    }

    double nested_time = omp_get_wtime() - nested_start;

    printf("\nПараллельная версия (вложенный параллелизм):\n");
    printf("  Максимум среди минимумов строк: %.2f\n", nested_result);
    printf("  Время: %.4f секунд\n", nested_time);
    printf("  Ускорение: %.2fx\n", seq_time / nested_time);

    // Проверка корректности
    printf("\nПроверка корректности:\n");
    printf("  Разница (редукция): %.10f\n", fabs(seq_result - red_result));
    printf("  Разница (вложенный): %.10f\n", fabs(seq_result - nested_result));

    // Освобождаем память
    free(row_minima);
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);

    return 0;
}
