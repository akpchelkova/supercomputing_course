#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <math.h>

// Функция для заполнения векторов случайными числами
void fill_vectors(double *vec1, double *vec2, int size) {
    for (int i = 0; i < size; i++) {
        vec1[i] = (double)rand() / RAND_MAX * 10.0;  // числа от 0 до 10
        vec2[i] = (double)rand() / RAND_MAX * 10.0;
    }
}

int main(int argc, char *argv[]) {
    // Размер векторов можно передавать как аргумент
    int size = 1000000;  // 1 миллион элементов по умолчанию
    if (argc > 1) {
        size = atoi(argv[1]);
    }

    // Выделяем память под векторы
    double *vec1 = (double*)malloc(size * sizeof(double));
    double *vec2 = (double*)malloc(size * sizeof(double));
    
    if (vec1 == NULL || vec2 == NULL) {
        printf("Ошибка выделения памяти!\n");
        return 1;
    }

    // Инициализируем генератор случайных чисел
    srand(time(NULL));
    fill_vectors(vec1, vec2, size);

    printf("Размер векторов: %d элементов\n", size);

    // ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ
    double seq_dot = 0.0;
    double seq_start = omp_get_wtime();
    
    for (int i = 0; i < size; i++) {
        seq_dot += vec1[i] * vec2[i];
    }
    
    double seq_time = omp_get_wtime() - seq_start;

    printf("Последовательная версия:\n");
    printf("  Скалярное произведение: %.2f\n", seq_dot);
    printf("  Время: %.4f секунд\n", seq_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ С РЕДУКЦИЕЙ
    double red_dot = 0.0;
    double red_start = omp_get_wtime();

    #pragma omp parallel for reduction(+:red_dot)
    for (int i = 0; i < size; i++) {
        red_dot += vec1[i] * vec2[i];
    }

    double red_time = omp_get_wtime() - red_start;

    printf("\nПараллельная версия (редукция):\n");
    printf("  Скалярное произведение: %.2f\n", red_dot);
    printf("  Время: %.4f секунд\n", red_time);
    printf("  Ускорение: %.2fx\n", seq_time / red_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ БЕЗ РЕДУКЦИИ (критические секции)
    double crit_dot = 0.0;
    double crit_start = omp_get_wtime();

    #pragma omp parallel
    {
        double local_dot = 0.0;
        
        // Каждый поток вычисляет свою часть скалярного произведения
        #pragma omp for
        for (int i = 0; i < size; i++) {
            local_dot += vec1[i] * vec2[i];
        }
        
        // Суммируем результаты через критические секции
        #pragma omp critical
        {
            crit_dot += local_dot;
        }
    }

    double crit_time = omp_get_wtime() - crit_start;

    printf("\nПараллельная версия (критические секции):\n");
    printf("  Скалярное произведение: %.2f\n", crit_dot);
    printf("  Время: %.4f секунд\n", crit_time);
    printf("  Ускорение: %.2fx\n", seq_time / crit_time);

    // Проверка корректности результатов
    printf("\nПроверка корректности:\n");
    printf("  Разница (редукция): %.10f\n", fabs(seq_dot - red_dot));
    printf("  Разница (крит.секции): %.10f\n", fabs(seq_dot - crit_dot));

    free(vec1);
    free(vec2);
    return 0;
}
