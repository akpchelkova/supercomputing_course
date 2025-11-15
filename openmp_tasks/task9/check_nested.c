#include <stdio.h>
#include <omp.h>

int main() {
    printf("ПРОВЕРКА ПОДДЕРЖКИ ВЛОЖЕННОГО ПАРАЛЛЕЛИЗМА\n");
    printf("==========================================\n");
    
    int nested_before = omp_get_nested();
    printf("Вложенный параллелизм до omp_set_nested(1): %s\n", 
           nested_before ? "ДА" : "НЕТ");
    
    omp_set_nested(1);
    
    int nested_after = omp_get_nested();
    printf("Вложенный параллелизм после omp_set_nested(1): %s\n", 
           nested_after ? "ДА" : "НЕТ");
    
    int max_levels = omp_get_max_active_levels();
    printf("Максимальное количество активных уровней: %d\n", max_levels);
    
    if (nested_after) {
        printf("\n✅ Вложенный параллелизм ПОДДЕРЖИВАЕТСЯ\n");
    } else {
        printf("\n❌ Вложенный параллелизм НЕ ПОДДЕРЖИВАЕТСЯ\n");
    }
    
    return 0;
}
