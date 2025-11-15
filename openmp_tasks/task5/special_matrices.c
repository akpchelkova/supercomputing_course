#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>
#include <string.h>

// Типы матриц
typedef enum {
    DENSE,      // Обычная плотная матрица
    TRIANGULAR, // Верхняя треугольная
    BANDED,     // Ленточная матрица
    SPARSE      // Разреженная (случайные нули)
} MatrixType;

// Функция для заполнения матрицы специального типа
void fill_special_matrix(double **matrix, int size, MatrixType type) {
    switch (type) {
        case DENSE:
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                }
            }
            break;
            
        case TRIANGULAR:
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    if (j >= i) {  // Верхняя треугольная
                        matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                    } else {
                        matrix[i][j] = 0.0;
                    }
                }
            }
            break;
            
        case BANDED:
            {
                int bandwidth = size / 10;  // Ширина ленты = 10% от размера
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        if (abs(i - j) <= bandwidth) {
                            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                        } else {
                            matrix[i][j] = 0.0;
                        }
                    }
                }
            }
            break;
            
        case SPARSE:
            {
                double sparsity = 0.1;  // 90% нулей
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        if ((double)rand() / RAND_MAX > sparsity) {
                            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                        } else {
                            matrix[i][j] = 0.0;
                        }
                    }
                }
            }
            break;
    }
}

// Функция для поиска максимума среди минимумов строк с разными schedule
double find_max_of_row_minima(double **matrix, int size, MatrixType type, const char* schedule_type) {
    double result = -1.0;
    
    #pragma omp parallel
    {
        double local_max = -1.0;
        
        if (strcmp(schedule_type, "static") == 0) {
            #pragma omp for schedule(static)
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;
                
                // Для разных типов матриц - разная вычислительная нагрузка
                switch (type) {
                    case DENSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                        
                    case TRIANGULAR:
                        for (int j = i; j < size; j++) {  // Только верхний треугольник
                            if (matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                        
                    case BANDED:
                        {
                            int bandwidth = size / 10;
                            int start = (i - bandwidth > 0) ? i - bandwidth : 0;
                            int end = (i + bandwidth < size) ? i + bandwidth : size - 1;
                            for (int j = start; j <= end; j++) {
                                if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                    row_min = matrix[i][j];
                                }
                            }
                        }
                        break;
                        
                    case SPARSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] != 0.0 && matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                }
                
                if (row_min > local_max && row_min < 1e9) {
                    local_max = row_min;
                }
            }
        }
        else if (strcmp(schedule_type, "dynamic") == 0) {
            #pragma omp for schedule(dynamic, 10)  // chunk size = 10
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;
                // ... тот же код что выше ...
                switch (type) {
                    case DENSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                    case TRIANGULAR:
                        for (int j = i; j < size; j++) {
                            if (matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                    case BANDED:
                        {
                            int bandwidth = size / 10;
                            int start = (i - bandwidth > 0) ? i - bandwidth : 0;
                            int end = (i + bandwidth < size) ? i + bandwidth : size - 1;
                            for (int j = start; j <= end; j++) {
                                if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                    row_min = matrix[i][j];
                                }
                            }
                        }
                        break;
                    case SPARSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] != 0.0 && matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                }
                
                if (row_min > local_max && row_min < 1e9) {
                    local_max = row_min;
                }
            }
        }
        else if (strcmp(schedule_type, "guided") == 0) {
            #pragma omp for schedule(guided)
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;
                // ... тот же код что выше ...
                switch (type) {
                    case DENSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                    case TRIANGULAR:
                        for (int j = i; j < size; j++) {
                            if (matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                    case BANDED:
                        {
                            int bandwidth = size / 10;
                            int start = (i - bandwidth > 0) ? i - bandwidth : 0;
                            int end = (i + bandwidth < size) ? i + bandwidth : size - 1;
                            for (int j = start; j <= end; j++) {
                                if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                    row_min = matrix[i][j];
                                }
                            }
                        }
                        break;
                    case SPARSE:
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] != 0.0 && matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                }
                
                if (row_min > local_max && row_min < 1e9) {
                    local_max = row_min;
                }
            }
        }
        
        #pragma omp critical
        {
            if (local_max > result) {
                result = local_max;
            }
        }
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    int size = 2000;  // размер матрицы по умолчанию
    if (argc > 1) {
        size = atoi(argv[1]);
    }

    // Выделяем память под матрицу
    double **matrix = (double**)malloc(size * sizeof(double*));
    for (int i = 0; i < size; i++) {
        matrix[i] = (double*)malloc(size * sizeof(double));
    }

    // Типы матриц для тестирования
    MatrixType types[] = {DENSE, TRIANGULAR, BANDED, SPARSE};
    const char* type_names[] = {"DENSE", "TRIANGULAR", "BANDED", "SPARSE"};
    
    // Типы распределения
    const char* schedules[] = {"static", "dynamic", "guided"};
    
    srand(time(NULL));

    printf("Исследование специальных типов матриц и распределения итераций\n");
    printf("Размер матрицы: %d x %d\n\n", size, size);

    for (int t = 0; t < 4; t++) {
        MatrixType current_type = types[t];
        
        // Заполняем матрицу специального типа
        fill_special_matrix(matrix, size, current_type);
        
        printf("=== Тип матрицы: %s ===\n", type_names[t]);
        
        // Последовательная версия для сравнения
        double seq_start = omp_get_wtime();
        double seq_result = find_max_of_row_minima(matrix, size, current_type, "static");
        double seq_time = omp_get_wtime() - seq_start;
        
        printf("Последовательная версия: %.2f (время: %.4f сек)\n", seq_result, seq_time);
        
        // Тестируем разные типы распределения
        for (int s = 0; s < 3; s++) {
            double par_start = omp_get_wtime();
            double par_result = find_max_of_row_minima(matrix, size, current_type, schedules[s]);
            double par_time = omp_get_wtime() - par_start;
            
            printf("  Schedule(%s): %.2f, время: %.4f сек, ускорение: %.2fx\n", 
                   schedules[s], par_result, par_time, seq_time / par_time);
        }
        printf("\n");
    }

    // Освобождаем память
    for (int i = 0; i < size; i++) {
        free(matrix[i]);
    }
    free(matrix);

    return 0;
}
