#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>
#include <string.h>

// типы матриц для экспериментов
typedef enum {
    DENSE,      // обычная плотная матрица (все элементы ненулевые)
    TRIANGULAR, // верхняя треугольная (нижний треугольник нулевой)
    BANDED,     // ленточная матрица (ненулевые элементы только возле диагонали)
    SPARSE      // разреженная матрица (случайные нули, 90% разреженность)
} MatrixType;

// функция для заполнения матрицы специального типа
void fill_special_matrix(double **matrix, int size, MatrixType type) {
    switch (type) {
        case DENSE:
            // плотная матрица - все элементы ненулевые
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    matrix[i][j] = (double)rand() / RAND_MAX * 100.0;  // случайные числа 0-100
                }
            }
            break;
            
        case TRIANGULAR:
            // верхняя треугольная матрица - нули ниже главной диагонали
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    if (j >= i) {  // верхний треугольник включая диагональ
                        matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                    } else {
                        matrix[i][j] = 0.0;  // нижний треугольник - нули
                    }
                }
            }
            break;
            
        case BANDED:
            // ленточная матрица - ненулевые элементы только в полосе вокруг диагонали
            {
                int bandwidth = size / 10;  // ширина ленты = 10% от размера матрицы
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        if (abs(i - j) <= bandwidth) {  // элементы в пределах полосы
                            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                        } else {
                            matrix[i][j] = 0.0;  // элементы вне полосы - нули
                        }
                    }
                }
            }
            break;
            
        case SPARSE:
            // разреженная матрица - большинство элементов нулевые
            {
                double sparsity = 0.1;  // только 10% элементов ненулевые (90% нулей)
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        if ((double)rand() / RAND_MAX > sparsity) {  // с вероятностью 10% создаем ненулевой элемент
                            matrix[i][j] = (double)rand() / RAND_MAX * 100.0;
                        } else {
                            matrix[i][j] = 0.0;  // 90% элементов - нули
                        }
                    }
                }
            }
            break;
    }
}

// функция для поиска максимума среди минимумов строк с разными schedule
double find_max_of_row_minima(double **matrix, int size, MatrixType type, const char* schedule_type) {
    double result = -1.0;  // инициализируем результат
    
    #pragma omp parallel
    {
        double local_max = -1.0;  // локальный максимум для каждого потока
        
        // статическое распределение итераций - равные блоки для каждого потока
        if (strcmp(schedule_type, "static") == 0) {
            #pragma omp for schedule(static)
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;  // большое начальное значение для поиска минимума
                
                // для разных типов матриц - разная вычислительная нагрузка на строку
                switch (type) {
                    case DENSE:
                        // плотная матрица - проверяем все элементы строки
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                        
                    case TRIANGULAR:
                        // треугольная матрица - проверяем только верхний треугольник
                        for (int j = i; j < size; j++) {  // начинаем с диагонали
                            if (matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                        
                    case BANDED:
                        // ленточная матрица - проверяем только элементы в полосе
                        {
                            int bandwidth = size / 10;
                            int start = (i - bandwidth > 0) ? i - bandwidth : 0;  // начало полосы
                            int end = (i + bandwidth < size) ? i + bandwidth : size - 1;  // конец полосы
                            for (int j = start; j <= end; j++) {
                                if (matrix[i][j] < row_min && matrix[i][j] != 0.0) {
                                    row_min = matrix[i][j];
                                }
                            }
                        }
                        break;
                        
                    case SPARSE:
                        // разреженная матрица - проверяем только ненулевые элементы
                        for (int j = 0; j < size; j++) {
                            if (matrix[i][j] != 0.0 && matrix[i][j] < row_min) {
                                row_min = matrix[i][j];
                            }
                        }
                        break;
                }
                
                // обновляем локальный максимум если нашли больший минимум строки
                if (row_min > local_max && row_min < 1e9) {
                    local_max = row_min;
                }
            }
        }
        // динамическое распределение - потоки берут небольшие порции итераций
        else if (strcmp(schedule_type, "dynamic") == 0) {
            #pragma omp for schedule(dynamic, 10)  // chunk size = 10 итераций
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;
                // аналогичный код обработки строк для разных типов матриц
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
        // guided распределение - размер порций уменьшается по мере выполнения
        else if (strcmp(schedule_type, "guided") == 0) {
            #pragma omp for schedule(guided)  // размер chunk уменьшается экспоненциально
            for (int i = 0; i < size; i++) {
                double row_min = 1e9;
                // аналогичный код обработки строк для разных типов матриц
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
        
        // критическая секция для безопасного обновления глобального результата
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
        size = atoi(argv[1]);  // можно передать размер как аргумент
    }

    // выделяем память под матрицу
    double **matrix = (double**)malloc(size * sizeof(double*));
    for (int i = 0; i < size; i++) {
        matrix[i] = (double*)malloc(size * sizeof(double));
    }

    // типы матриц для тестирования
    MatrixType types[] = {DENSE, TRIANGULAR, BANDED, SPARSE};
    const char* type_names[] = {"DENSE", "TRIANGULAR", "BANDED", "SPARSE"};
    
    // типы распределения итераций между потоками
    const char* schedules[] = {"static", "dynamic", "guided"};
    
    srand(time(NULL));  // инициализация генератора случайных чисел

    printf("исследование специальных типов матриц и распределения итераций\n");
    printf("размер матрицы: %d x %d\n\n", size, size);

    // тестируем все комбинации типов матриц и распределений
    for (int t = 0; t < 4; t++) {
        MatrixType current_type = types[t];
        
        // заполняем матрицу специального типа
        fill_special_matrix(matrix, size, current_type);
        
        printf("=== тип матрицы: %s ===\n", type_names[t]);
        
        // последовательная версия для сравнения (используем static без параллелизма)
        double seq_start = omp_get_wtime();
        double seq_result = find_max_of_row_minima(matrix, size, current_type, "static");
        double seq_time = omp_get_wtime() - seq_start;
        
        printf("последовательная версия: %.2f (время: %.4f сек)\n", seq_result, seq_time);
        
        // тестируем разные типы распределения в параллельной версии
        for (int s = 0; s < 3; s++) {
            double par_start = omp_get_wtime();
            double par_result = find_max_of_row_minima(matrix, size, current_type, schedules[s]);
            double par_time = omp_get_wtime() - par_start;
            
            printf("  schedule(%s): %.2f, время: %.4f сек, ускорение: %.2fx\n", 
                   schedules[s], par_result, par_time, seq_time / par_time);
        }
        printf("\n");
    }

    // освобождаем память
    for (int i = 0; i < size; i++) {
        free(matrix[i]);
    }
    free(matrix);

    return 0;
}
