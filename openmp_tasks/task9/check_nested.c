#include <stdio.h>
#include <omp.h>

int main() {
    printf("проверка поддержки вложенного параллелизма\n");
    printf("==========================================\n");
    
    int nested_before = omp_get_nested();
    printf("вложенный параллелизм до omp_set_nested(1): %s\n", 
           nested_before ? "да" : "нет");
    
    omp_set_nested(1);
    
    int nested_after = omp_get_nested();
    printf("вложенный параллелизм после omp_set_nested(1): %s\n", 
           nested_after ? "да" : "нет");
    
    int max_levels = omp_get_max_active_levels();
    printf("максимальное количество активных уровней: %d\n", max_levels);
    
    if (nested_after) {
        printf("\nвложенный параллелизм поддерживается\n");
    } else {
        printf("\nвложенный параллелизм не поддерживается\n");
    }
    
    return 0;
}
