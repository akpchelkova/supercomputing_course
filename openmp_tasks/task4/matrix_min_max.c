#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>

// функция для заполнения матрицы случайными числами
void fill_matrix(double **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;  // генерируем числа от 0 до 100
        }
    }
}

// функция для вывода матрицы (для маленьких размеров)
void print_matrix(double **matrix, int rows, int cols) {
    if (rows > 10 || cols > 10) {
        printf("матрица слишком большая для вывода\n");
        return;
    }
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%6.1f ", matrix[i][j]);  // форматированный вывод элементов
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    // размер матрицы можно передавать как аргументы командной строки
    int rows = 1000;    // количество строк по умолчанию
    int cols = 1000;    // количество столбцов по умолчанию
    
    if (argc > 2) {
        rows = atoi(argv[1]);  // преобразуем первый аргумент в число строк
        cols = atoi(argv[2]);  // преобразуем второй аргумент в число столбцов
    }

    // выделяем память под матрицу (массив указателей на строки)
    double **matrix = (double**)malloc(rows * sizeof(double*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (double*)malloc(cols * sizeof(double));  // выделяем память для каждой строки
    }

    // инициализируем генератор случайных чисел текущим временем
    srand(time(NULL));
    fill_matrix(matrix, rows, cols);  // заполняем матрицу случайными значениями

    printf("размер матрицы: %d x %d\n", rows, cols);
    
    // выводим матрицу только если она маленькая (для отладки)
    if (rows <= 5 && cols <= 5) {
        printf("матрица:\n");
        print_matrix(matrix, rows, cols);
    }

    // последовательная версия алгоритма
    double seq_result = 0.0;  // переменная для результата
    double seq_start = omp_get_wtime();  // засекаем время начала выполнения
    
    // находим минимумы для каждой строки матрицы
    double *row_minima = (double*)malloc(rows * sizeof(double));  // массив для хранения минимумов строк
    for (int i = 0; i < rows; i++) {
        double min_val = matrix[i][0];  // начинаем с первого элемента строки
        for (int j = 1; j < cols; j++) {
            if (matrix[i][j] < min_val) {
                min_val = matrix[i][j];  // обновляем минимум если нашли меньший элемент
            }
        }
        row_minima[i] = min_val;  // сохраняем минимум текущей строки
    }
    
    // находим максимум среди минимумов строк
    seq_result = row_minima[0];  // начинаем с первого минимума
    for (int i = 1; i < rows; i++) {
        if (row_minima[i] > seq_result) {
            seq_result = row_minima[i];  // обновляем максимум если нашли больший минимум
        }
    }
    
    double seq_time = omp_get_wtime() - seq_start;  // вычисляем время выполнения

    printf("\nпоследовательная версия:\n");
    printf("  максимум среди минимумов строк: %.2f\n", seq_result);
    printf("  время: %.4f секунд\n", seq_time);

    // параллельная версия с редукцией (внешний параллелизм)
    double red_result = 0.0;  // переменная для результата
    double red_start = omp_get_wtime();  // засекаем время начала

    #pragma omp parallel
    {
        double local_max = -1.0;  // локальный максимум для текущего потока
        
        // распределяем строки матрицы между потоками
        #pragma omp for
        for (int i = 0; i < rows; i++) {
            // находим минимум в текущей строке (последовательно)
            double row_min = matrix[i][0];
            for (int j = 1; j < cols; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];
                }
            }
            
            // обновляем локальный максимум для текущего потока
            if (row_min > local_max) {
                local_max = row_min;
            }
        }
        
        // критическая секция для безопасного обновления глобального результата
        #pragma omp critical
        {
            if (local_max > red_result) {
                red_result = local_max;
            }
        }
    }

    double red_time = omp_get_wtime() - red_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (редукция):\n");
    printf("  максимум среди минимумов строк: %.2f\n", red_result);
    printf("  время: %.4f секунд\n", red_time);
    printf("  ускорение: %.2fx\n", seq_time / red_time);  // вычисляем ускорение

    // параллельная версия с вложенным параллелизмом
    double nested_result = 0.0;  // переменная для результата
    double nested_start = omp_get_wtime();  // засекаем время начала

    #pragma omp parallel
    {
        double local_max = -1.0;  // локальный максимум для текущего потока
        
        // распараллеливаем внешний цикл по строкам
        #pragma omp for
        for (int i = 0; i < rows; i++) {
            double row_min = matrix[i][0];
            
            // внутренний цикл тоже распараллеливаем (вложенный параллелизм)
            #pragma omp parallel for reduction(min:row_min)
            for (int j = 0; j < cols; j++) {
                if (matrix[i][j] < row_min) {
                    row_min = matrix[i][j];  // редукция для нахождения минимума строки
                }
            }
            
            // обновляем локальный максимум для текущего потока
            #pragma omp critical
            {
                if (row_min > local_max) {
                    local_max = row_min;
                }
            }
        }
        
        // критическая секция для обновления глобального результата
        #pragma omp critical
        {
            if (local_max > nested_result) {
                nested_result = local_max;
            }
        }
    }

    double nested_time = omp_get_wtime() - nested_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (вложенный параллелизм):\n");
    printf("  максимум среди минимумов строк: %.2f\n", nested_result);
    printf("  время: %.4f секунд\n", nested_time);
    printf("  ускорение: %.2fx\n", seq_time / nested_time);  // вычисляем ускорение

    // проверка корректности результатов всех версий
    printf("\nпроверка корректности:\n");
    printf("  разница (редукция): %.10f\n", fabs(seq_result - red_result));  // сравнение с последовательной версией
    printf("  разница (вложенный): %.10f\n", fabs(seq_result - nested_result));  // сравнение с последовательной версией

    // освобождаем память
    free(row_minima);  // освобождаем массив минимумов строк
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);  // освобождаем каждую строку матрицы
    }
    free(matrix);  // освобождаем массив указателей

    return 0;
}
