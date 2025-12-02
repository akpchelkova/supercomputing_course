#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

// подынтегральная функция - можно менять для разных экспериментов
double f(double x) {
    return sin(x);  // ∫sin(x)dx от 0 до π = 2
    // return x * x; // ∫x²dx от 0 до 1 = 1/3
    // return exp(x); // ∫e^xdx от 0 до 1 = e - 1 ≈ 1.718
}

int main(int argc, char *argv[]) {
    // параметры интегрирования
    double a = 0.0;          // нижний предел интегрирования
    double b = M_PI;         // верхний предел интегрирования (π)
    int n = 100000000;       // количество разбиений по умолчанию
    
    if (argc > 1) {
        n = atoi(argv[1]);   // можно передать количество разбиений как аргумент командной строки
    }
    
    double h = (b - a) / n;  // вычисляем шаг интегрирования
    double exact_value = 2.0; // точное значение ∫sin(x)dx от 0 до π = 2

    printf("вычисление интеграла ∫sin(x)dx от %.2f до %.2f\n", a, b);
    printf("количество разбиений: %d\n", n);
    printf("шаг h: %.10f\n", h);
    printf("точное значение: %.10f\n", exact_value);

    // последовательная версия (метод прямоугольников)
    double seq_integral = 0.0;  // переменная для накопления результата
    double seq_start = omp_get_wtime();  // засекаем время начала выполнения
    
    // метод средних прямоугольников: используем значение функции в середине интервала
    for (int i = 0; i < n; i++) {
        double x = a + (i + 0.5) * h;  // вычисляем x в середине интервала
        seq_integral += f(x) * h;       // добавляем площадь прямоугольника
    }
    
    double seq_time = omp_get_wtime() - seq_start;  // вычисляем время выполнения

    printf("\nпоследовательная версия:\n");
    printf("  приближенное значение: %.10f\n", seq_integral);
    printf("  погрешность: %.10f\n", fabs(seq_integral - exact_value));  // абсолютная погрешность
    printf("  время: %.4f секунд\n", seq_time);

    // параллельная версия с использованием редукции
    double red_integral = 0.0;  // переменная для накопления результата
    double red_start = omp_get_wtime();  // засекаем время начала

    // директива openmp для параллельного цикла с редукцией сложения
    #pragma omp parallel for reduction(+:red_integral)
    for (int i = 0; i < n; i++) {
        double x = a + (i + 0.5) * h;  // каждый поток вычисляет свою точку
        red_integral += f(x) * h;       // каждый поток накапливает свою часть суммы
    }
    // openmp автоматически суммирует результаты всех потоков

    double red_time = omp_get_wtime() - red_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (редукция):\n");
    printf("  приближенное значение: %.10f\n", red_integral);
    printf("  погрешность: %.10f\n", fabs(red_integral - exact_value));
    printf("  время: %.4f секунд\n", red_time);
    printf("  ускорение: %.2fx\n", seq_time / red_time);  // вычисляем ускорение

    // параллельная версия без редукции с использованием критических секций
    double crit_integral = 0.0;  // переменная для накопления результата
    double crit_start = omp_get_wtime();  // засекаем время начала

    #pragma omp parallel
    {
        double local_integral = 0.0;  // локальная переменная для каждого потока
        
        // каждый поток вычисляет свою часть интеграла
        #pragma omp for
        for (int i = 0; i < n; i++) {
            double x = a + (i + 0.5) * h;  // вычисляем точку в середине интервала
            local_integral += f(x) * h;     // накапливаем локальную сумму
        }
        
        // суммируем результаты всех потоков через критические секции
        #pragma omp critical
        {
            crit_integral += local_integral;  // безопасное добавление к глобальному результату
        }
    }

    double crit_time = omp_get_wtime() - crit_start;  // вычисляем время выполнения

    printf("\nпараллельная версия (критические секции):\n");
    printf("  приближенное значение: %.10f\n", crit_integral);
    printf("  погрешность: %.10f\n", fabs(crit_integral - exact_value));
    printf("  время: %.4f секунд\n", crit_time);
    printf("  ускорение: %.2fx\n", seq_time / crit_time);  // вычисляем ускорение

    // сравнение численных результатов методов между собой
    printf("\nсравнение методов:\n");
    printf("  разница (редукция): %.10f\n", fabs(seq_integral - red_integral));  // сравнение с последовательной версией
    printf("  разница (крит.секции): %.10f\n", fabs(seq_integral - crit_integral));  // сравнение с последовательной версией

    return 0;
}
