#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_VECTORS 8
#define VECTOR_SIZE 1000000

int main() {
    FILE *file_a, *file_b;
    
    srand(time(NULL));
    
    // Создаем файл с векторами A
    file_a = fopen("vectors_a.dat", "wb");
    printf("Генерация vectors_a.dat...\n");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_a);
        }
    }
    fclose(file_a);
    
    // Создаем файл с векторами B  
    file_b = fopen("vectors_b.dat", "wb");
    printf("Генерация vectors_b.dat...\n");
    for (int i = 0; i < NUM_VECTORS; i++) {
        for (int j = 0; j < VECTOR_SIZE; j++) {
            double value = (double)rand() / RAND_MAX * 10.0;
            fwrite(&value, sizeof(double), 1, file_b);
        }
    }
    fclose(file_b);
    
    printf("Файлы созданы: vectors_a.dat, vectors_b.dat\n");
    printf("Размер каждого файла: %ld MB\n", 
           (long)(NUM_VECTORS * VECTOR_SIZE * sizeof(double)) / (1024*1024));
    return 0;
}
