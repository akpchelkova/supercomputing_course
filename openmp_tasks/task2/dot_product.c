#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>

// функция для заполнения векторов случайными числами
void fill_vectors(double *vec1, double *vec2, int size) {
    for (int i = 0; i < size; i++) {
        vec1[i] = (double)rand() / RAND_MAX * 10.0;  // генерируем числа от 0 до 10 для первого вектора
        vec2[i] = (double)rand() / RAND_MAX * 10.0;  // генерируем числа от 0 до 10 для второго вектора
    }
}

int main(int argc, char *argv[]) {
    // размер векторов можно передавать как аргумент командной строки
    int size = 1000000;  // значение по умолчанию - 1 миллион элементов
    if (argc > 1) {
        size = atoi(argv[1]);  // преобразуем строковый аргумент в число
    }

    // выделяем память под два вектора типа double
    double *vec1 = (double*)malloc(size * sizeof(double));
    double *vec2 = (double*)malloc(size * sizeof(double));
    
    if (vec1 == NULL || vec2 == NULL) {
        printf("ошибка выделения памяти!\n");
        return 1;
    }

    // инициализируем генератор случайных чисел текущим временем
    srand(time(NULL));
    fill_vectors(vec1, vec2, size);  // заполняем векторы случайными значениями

    printf("размер векторов: %d элементов\n", size);

    // последовательная версия вычисления скалярного произведения
    double seq_dot = 0.0;  // переменная для хранения результата
    double seq_start = omp_get_wtime();  // засекаем время начала выполнения
    
    // последовательный перебор всех элементов векторов
    for (int i = 0; i < size; i++) {
        seq_dot += vec1[i] * vec2[i];  // накапливаем сумму произведений соответствующих элементов
    }
    
    double seq_time = omp_get_wtime() - seq_start;  // вычисляем время выполнения

    printf("последовательная версия:\n");
    printf("  скалярное произведение: %.2f\n", seq_dot);
    printf("  время: %.4f секунд\n", seq_time);

    // параллельная версия с использованием редукции
    double red_dot = 0.0;  // переменная для хранения результата
    double red_start = omp_get_wtime();  // засекаем время начала

    // директива openmp для параллельного цикла с редукцией сложения
    #pragma omp parallel for reduction(+:red_dot)
    for (int i = 0; i < size; i++) {
        red_dot += vec1[i] * vec2[i];  // каждый поток вычисляет свою часть суммы
    }
    // openmp автоматически суммирует результаты всех потоков

    double red_time = omp_get_wtime() - red_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (редукция):\n");
    printf("  скалярное произведение: %.2f\n", red_dot);
    printf("  время: %.4f секунд\n", red_time);
    printf("  ускорение: %.2fx\n", seq_time / red_time);  // вычисляем ускорение

    // параллельная версия без редукции с использованием критических секций
    double crit_dot = 0.0;  // переменная для хранения результата
    double crit_start = omp_get_wtime();  // засекаем время начала

    #pragma omp parallel
    {
        double local_dot = 0.0;  // локальная переменная для каждого потока
        
        // каждый поток вычисляет свою часть скалярного произведения
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_dot += vec1[i] * vec2[i];  // поток накапливает сумму в своей локальной переменной
        }
        
        // суммируем результаты всех потоков через критические секции
        #pragma omp critical
        {
            crit_dot += local_dot;  // безопасное добавление локальной суммы к глобальному результату
        }
    }

    double crit_time = omp_get_wtime() - crit_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (критические секции):\n");
    printf("  скалярное произведение: %.2f\n", crit_dot);
    printf("  время: %.4f секунд\n", crit_time);
    printf("  ускорение: %.2fx\n", seq_time / crit_time);  // вычисляем ускорение

    // проверка корректности результатов всех трех версий
    printf("\nпроверка корректности:\n");
    printf("  разница (редукция): %.10f\n", fabs(seq_dot - red_dot));  // сравниваем с последовательной версией
    printf("  разница (крит.секции): %.10f\n", fabs(seq_dot - crit_dot));  // сравниваем с последовательной версией

    free(vec1);  // освобождаем память, выделенную под первый вектор
    free(vec2);  // освобождаем память, выделенную под второй вектор
    return 0;
}
