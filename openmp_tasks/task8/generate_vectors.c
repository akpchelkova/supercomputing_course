#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_VECTORS 8
#define VECTOR_SIZE 1000000

int main() {
    FILE *file_a, *file_b;
    
    srand(time(NULL));  // инициализация генератора случайных чисел
    
    // создаем файл с векторами A
    file_a = fopen("vectors_a.dat", "wb");
    printf("генерация vectors_a.dat...\n");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;  // случайные числа 0-10
            fwrite(&value, sizeof(double), 1, file_a);
        }
    }
    fclose(file_a);
    
    // создаем файл с векторами B  
    file_b = fopen("vectors_b.dat", "wb");
    printf("генерация vectors_b.dat...\n");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;  // случайные числа 0-10
            fwrite(&value, sizeof(double), 1, file_b);
        }
    }
    fclose(file_b);
    
    printf("файлы созданы: vectors_a.dat, vectors_b.dat\n");
    printf("размер каждого файла: %ld MB\n", 
           (long)(NUM_VECTORS * VECTOR_SIZE * sizeof(double)) / (1024*1024));
    return 0;
}
