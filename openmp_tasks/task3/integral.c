#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

// Подынтегральная функция - можно менять для разных экспериментов
double f(double x) {
    return sin(x);  // ∫sin(x)dx от 0 до π = 2
    // return x * x; // ∫x²dx от 0 до 1 = 1/3
    // return exp(x); // ∫e^xdx от 0 до 1 = e - 1 ≈ 1.718
}

int main(int argc, char *argv[]) {
    // Параметры интегрирования
    double a = 0.0;          // нижний предел
    double b = M_PI;         // верхний предел (π)
    int n = 100000000;       // количество разбиений по умолчанию
    
    if (argc > 1) {
        n = atoi(argv[1]);   // можно передать количество разбиений как аргумент
    }
    
    double h = (b - a) / n;  // шаг интегрирования
    double exact_value = 2.0; // точное значение ∫sin(x)dx от 0 до π = 2

    printf("Вычисление интеграла ∫sin(x)dx от %.2f до %.2f\n", a, b);
    printf("Количество разбиений: %d\n", n);
    printf("Шаг h: %.10f\n", h);
    printf("Точное значение: %.10f\n", exact_value);

    // ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ (метод прямоугольников)
    double seq_integral = 0.0;
    double seq_start = omp_get_wtime();
    
    for (int i = 0; i < n; i++) {
        double x = a + (i + 0.5) * h;  // середина интервала
        seq_integral += f(x) * h;
    }
    
    double seq_time = omp_get_wtime() - seq_start;

    printf("\nПоследовательная версия:\n");
    printf("  Приближенное значение: %.10f\n", seq_integral);
    printf("  Погрешность: %.10f\n", fabs(seq_integral - exact_value));
    printf("  Время: %.4f секунд\n", seq_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ С РЕДУКЦИЕЙ
    double red_integral = 0.0;
    double red_start = omp_get_wtime();

    #pragma omp parallel for reduction(+:red_integral)
    for (int i = 0; i < n; i++) {
        double x = a + (i + 0.5) * h;
        red_integral += f(x) * h;
    }

    double red_time = omp_get_wtime() - red_start;

    printf("\nПараллельная версия (редукция):\n");
    printf("  Приближенное значение: %.10f\n", red_integral);
    printf("  Погрешность: %.10f\n", fabs(red_integral - exact_value));
    printf("  Время: %.4f секунд\n", red_time);
    printf("  Ускорение: %.2fx\n", seq_time / red_time);

    // ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ БЕЗ РЕДУКЦИИ (критические секции)
    double crit_integral = 0.0;
    double crit_start = omp_get_wtime();

    #pragma omp parallel
    {
        double local_integral = 0.0;
        
        // Каждый поток вычисляет свою часть интеграла
        #pragma omp for
        for (int i = 0; i < n; i++) {
            double x = a + (i + 0.5) * h;
            local_integral += f(x) * h;
        }
        
        // Суммируем результаты через критические секции
        #pragma omp critical
        {
            crit_integral += local_integral;
        }
    }

    double crit_time = omp_get_wtime() - crit_start;

    printf("\nПараллельная версия (критические секции):\n");
    printf("  Приближенное значение: %.10f\n", crit_integral);
    printf("  Погрешность: %.10f\n", fabs(crit_integral - exact_value));
    printf("  Время: %.4f секунд\n", crit_time);
    printf("  Ускорение: %.2fx\n", seq_time / crit_time);

    // Сравнение методов
    printf("\nСравнение методов:\n");
    printf("  Разница (редукция): %.10f\n", fabs(seq_integral - red_integral));
    printf("  Разница (крит.секции): %.10f\n", fabs(seq_integral - crit_integral));

    return 0;
}
