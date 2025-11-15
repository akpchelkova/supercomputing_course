#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// Функция для заполнения массива случайными числами
void fill_array(double *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = (double)rand() / RAND_MAX * 1000.0;  // числа от 0 до 1000
    }
}

int main(int argc, char *argv[]) {
    // Размер массива можно передавать как аргумент
    int size = 1000000;  // 1 миллион элементов по умолчанию
    if (argc > 1) {
        size = atoi(argv[1]);
    }
    
    // Выделяем память под массив
    double *array = (double*)malloc(size * sizeof(double));
    if (array == NULL) {
        printf("Ошибка выделения памяти!\n");
        return 1;
    }
    
    // Инициализируем генератор случайных чисел
    srand(time(NULL));
    fill_array(array, size);
    
    printf("Размер массива: %d элементов\n", size);
    
    // Здесь будем добавлять разные версии алгоритмов

        // ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ
    double seq_min = array[0];
    double seq_max = array[0];
    double seq_start = omp_get_wtime();
    
    for (int i = 0; i < size; i++) {
        if (array[i] < seq_min) seq_min = array[i];
        if (array[i] > seq_max) seq_max = array[i];
    }
    
    double seq_time = omp_get_wtime() - seq_start;
    
    printf("Последовательная версия:\n");
    printf("  Минимум: %.2f, Максимум: %.2f\n", seq_min, seq_max);
    printf("  Время: %.4f секунд\n", seq_time);

        // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ С РЕДУКЦИЕЙ
    double red_min = array[0];
    double red_max = array[0];
    double red_start = omp_get_wtime();
    
    #pragma omp parallel for reduction(min:red_min) reduction(max:red_max)
    for (int i = 0; i < size; i++) {
        if (array[i] < red_min) red_min = array[i];
        if (array[i] > red_max) red_max = array[i];
    }
    
    double red_time = omp_get_wtime() - red_start;
    
    printf("\nПараллельная версия (редукция):\n");
    printf("  Минимум: %.2f, Максимум: %.2f\n", red_min, red_max);
    printf("  Время: %.4f секунд\n", red_time);
    printf("  Ускорение: %.2fx\n", seq_time / red_time);

        // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ БЕЗ РЕДУКЦИИ (критические секции)
    double crit_min = array[0];
    double crit_max = array[0];
    double crit_start = omp_get_wtime();
    
    #pragma omp parallel
    {
        double local_min = array[0];
        double local_max = array[0];
        
        // Каждый поток находит минимум/максимум в своей части массива
        #pragma omp for
        for (int i = 0; i < size; i++) {
            if (array[i] < local_min) local_min = array[i];
            if (array[i] > local_max) local_max = array[i];
        }
        
        // Обновляем глобальные значения через критические секции
        #pragma omp critical
        {
            if (local_min < crit_min) crit_min = local_min;
        }
        
        #pragma omp critical
        {
            if (local_max > crit_max) crit_max = local_max;
        }
    }
    
    double crit_time = omp_get_wtime() - crit_start;
    
    printf("\nПараллельная версия (критические секции):\n");
    printf("  Минимум: %.2f, Максимум: %.2f\n", crit_min, crit_max);
    printf("  Время: %.4f секунд\n", crit_time);
    printf("  Ускорение: %.2fx\n", seq_time / crit_time);

    free(array);
    return 0;
}
