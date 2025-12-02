#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// функция для заполнения массива случайными числами
void fill_array(double *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = (double)rand() / RAND_MAX * 1000.0;  // генерируем числа от 0 до 1000
    }
}

int main(int argc, char *argv[]) {
    // размер массива можно передавать как аргумент командной строки
    int size = 1000000;  // значение по умолчанию - 1 миллион элементов
    if (argc > 1) {
        size = atoi(argv[1]);  // преобразуем строковый аргумент в число
    }
    
    // выделяем память под массив типа double
    double *array = (double*)malloc(size * sizeof(double));
    if (array == NULL) {
        printf("ошибка выделения памяти!\n");
        return 1;
    }
    
    // инициализируем генератор случайных чисел текущим временем
    srand(time(NULL));
    fill_array(array, size);  // заполняем массив случайными значениями
    
    printf("размер массива: %d элементов\n", size);
    
    // здесь будем добавлять разные версии алгоритмов

        // последовательная версия поиска минимума и максимума
    double seq_min = array[0];  // начальное значение минимума - первый элемент
    double seq_max = array[0];  // начальное значение максимума - первый элемент
    double seq_start = omp_get_wtime();  // засекаем время начала выполнения
    
    // последовательный перебор всех элементов массива
    for (int i = 0; i < size; i++) {
        if (array[i] < seq_min) seq_min = array[i];  // обновляем минимум если нашли меньший элемент
        if (array[i] > seq_max) seq_max = array[i];  // обновляем максимум если нашли больший элемент
    }
    
    double seq_time = omp_get_wtime() - seq_start;  // вычисляем время выполнения
    
    printf("последовательная версия:\n");
    printf("  минимум: %.2f, максимум: %.2f\n", seq_min, seq_max);
    printf("  время: %.4f секунд\n", seq_time);

        // параллельная версия с использованием редукции
    double red_min = array[0];  // начальное значение минимума
    double red_max = array[0];  // начальное значение максимума
    double red_start = omp_get_wtime();  // засекаем время начала
    
    // директива openmp для параллельного цикла с редукцией
    #pragma omp parallel for reduction(min:red_min) reduction(max:red_max)
    for (int i = 0; i < size; i++) {
        if (array[i] < red_min) red_min = array[i];  // каждый поток находит локальный минимум
        if (array[i] > red_max) red_max = array[i];  // каждый поток находит локальный максимум
    }
    // openmp автоматически объединяет результаты всех потоков
    
    double red_time = omp_get_wtime() - red_start;  // вычисляем время выполнения
    
    printf("\nпараллельная версия (редукция):\n");
    printf("  минимум: %.2f, максимум: %.2f\n", red_min, red_max);
    printf("  время: %.4f секунд\n", red_time);
    printf("  ускорение: %.2fx\n", seq_time / red_time);  // вычисляем ускорение

        // параллельная версия без редукции с использованием критических секций
    double crit_min = array[0];  // начальное значение минимума
    double crit_max = array[0];  // начальное значение максимума
    double crit_start = omp_get_wtime();  // засекаем время начала
    
    #pragma omp parallel
    {
        double local_min = array[0];  // локальный минимум для каждого потока
        double local_max = array[0];  // локальный максимум для каждого потока
        
        // распределяем итерации цикла между потоками
        #pragma omp for
        for (int i = 0; i < size; i++) {
            if (array[i] < local_min) local_min = array[i];  // поток находит минимум в своей части
            if (array[i] > local_max) local_max = array[i];  // поток находит максимум в своей части
        }
        
        // критическая секция для безопасного обновления глобального минимума
        #pragma omp critical
        {
            if (local_min < crit_min) crit_min = local_min;  // обновляем если нашли меньший минимум
        }
        
        // критическая секция для безопасного обновления глобального максимума
        #pragma omp critical
        {
            if (local_max > crit_max) crit_max = local_max;  // обновляем если нашли больший максимум
        }
    }
    
    double crit_time = omp_get_wtime() - crit_start;  // вычисляем время выполнения
    
    printf("\nпараллельная версия (критические секции):\n");
    printf("  минимум: %.2f, максимум: %.2f\n", crit_min, crit_max);
    printf("  время: %.4f секунд\n", crit_time);
    printf("  ускорение: %.2fx\n", seq_time / crit_time);  // вычисляем ускорение

    free(array);  // освобождаем память, выделенную под массив
    return 0;
}
