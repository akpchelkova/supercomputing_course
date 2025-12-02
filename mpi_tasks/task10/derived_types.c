#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// структура для тестирования - представляет частицу с различными типами данных
typedef struct {
    int id;              // идентификатор частицы
    double values[3];    // массив из трех вещественных чисел
    char name[20];       // строковое имя частицы
} Particle;

// функция для создания производного типа данных mpi для структуры Particle
MPI_Datatype create_particle_type() {
    MPI_Datatype particle_type;
    
    // определяем количество элементов в каждом блоке структуры
    int blocklengths[3] = {1, 3, 20};  // id (1 int), values[3] (3 double), name[20] (20 char)
    MPI_Datatype types[3] = {MPI_INT, MPI_DOUBLE, MPI_CHAR};
    
    // вычисляем смещения для каждого поля структуры
    MPI_Aint displacements[3];
    Particle dummy;  // временная переменная для вычисления смещений
    
    MPI_Get_address(&dummy.id, &displacements[0]);
    MPI_Get_address(&dummy.values[0], &displacements[1]);
    MPI_Get_address(&dummy.name[0], &displacements[2]);
    
    // преобразуем абсолютные смещения в относительные (относительно начала структуры)
    displacements[1] -= displacements[0];
    displacements[2] -= displacements[0];
    displacements[0] = 0;
    
    // создаем структурированный производный тип данных
    MPI_Type_create_struct(3, blocklengths, displacements, types, &particle_type);
    MPI_Type_commit(&particle_type);  // фиксируем тип для использования в mpi
    
    return particle_type;
}

// объявляем функции заранее для компиляции
void test_contiguous_type(int rank, int size);

// тест передачи данных с использованием производного типа
double test_derived_type(int num_particles, int num_iterations, int rank, int size) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    
    // инициализация тестовых данных случайными значениями
    srand(time(NULL) + rank);
    for (int i = 0; i < num_particles; i++) {
        particles[i].id = rank * 1000 + i;  // уникальный id для каждой частицы
        for (int j = 0; j < 3; j++) {
            particles[i].values[j] = (double)rand() / RAND_MAX * 100.0;
        }
        sprintf(particles[i].name, "particle_%d_%d", rank, i);
    }
    
    // создаем производный тип для структуры Particle
    MPI_Datatype particle_type = create_particle_type();
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // передача данных с использованием производного типа
    for (int iter = 0; iter < num_iterations; iter++) {
        if (rank == 0) {
            // процесс 0 отправляет данные всем остальным процессам
            for (int i = 1; i < size; i++) {
                MPI_Send(particles, num_particles, particle_type, i, 0, MPI_COMM_WORLD);
            }
        } else {
            // остальные процессы получают данные от процесса 0
            MPI_Recv(particles, num_particles, particle_type, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    double end_time = MPI_Wtime();
    double derived_time = end_time - start_time;
    
    // освобождаем производный тип
    MPI_Type_free(&particle_type);
    
    if (rank == 0) {
        printf("Derived type: %.6f seconds for %d particles, %d iterations\n",
               derived_time, num_particles, num_iterations);
    }
    
    free(particles);
    return derived_time;
}

// тест с ручной упаковкой и распаковкой данных
void test_packing(int num_particles, int num_iterations, int rank, int size, double derived_time) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    char* pack_buffer = malloc(num_particles * sizeof(Particle) * 2); // буфер с запасом места
    
    // инициализация данных (другие случайные значения для чистоты эксперимента)
    srand(time(NULL) + rank + 1000);
    for (int i = 0; i < num_particles; i++) {
        particles[i].id = rank * 1000 + i;
        for (int j = 0; j < 3; j++) {
            particles[i].values[j] = (double)rand() / RAND_MAX * 100.0;
        }
        sprintf(particles[i].name, "particle_%d_%d", rank, i);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        if (rank == 0) {
            // упаковка и отправка данных
            for (int i = 1; i < size; i++) {
                int position = 0;  // текущая позиция в буфере упаковки
                MPI_Pack(&num_particles, 1, MPI_INT, pack_buffer, 
                         num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                
                // упаковываем каждую частицу по полям
                for (int j = 0; j < num_particles; j++) {
                    MPI_Pack(&particles[j].id, 1, MPI_INT, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                    MPI_Pack(particles[j].values, 3, MPI_DOUBLE, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                    MPI_Pack(particles[j].name, 20, MPI_CHAR, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                }
                
                // отправляем упакованные данные
                MPI_Send(pack_buffer, position, MPI_PACKED, i, 0, MPI_COMM_WORLD);
            }
        } else {
            // получение и распаковка данных
            int recv_size;
            MPI_Status status;
            MPI_Probe(0, 0, MPI_COMM_WORLD, &status);  // проверяем размер сообщения
            MPI_Get_count(&status, MPI_PACKED, &recv_size);
            
            char* recv_buffer = malloc(recv_size);
            MPI_Recv(recv_buffer, recv_size, MPI_PACKED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            int position = 0;
            int received_count;
            MPI_Unpack(recv_buffer, recv_size, &position, &received_count, 1, MPI_INT, MPI_COMM_WORLD);
            
            // распаковываем каждую частицу
            for (int j = 0; j < received_count; j++) {
                MPI_Unpack(recv_buffer, recv_size, &position, &particles[j].id, 1, MPI_INT, MPI_COMM_WORLD);
                MPI_Unpack(recv_buffer, recv_size, &position, particles[j].values, 3, MPI_DOUBLE, MPI_COMM_WORLD);
                MPI_Unpack(recv_buffer, recv_size, &position, particles[j].name, 20, MPI_CHAR, MPI_COMM_WORLD);
            }
            
            free(recv_buffer);
        }
    }
    
    double end_time = MPI_Wtime();
    double packing_time = end_time - start_time;
    
    if (rank == 0) {
        printf("Packing:      %.6f seconds for %d particles, %d iterations\n",
               packing_time, num_particles, num_iterations);
        printf("Ratio (Packing/Derived): %.2f\n", packing_time / derived_time);
    }
    
    free(particles);
    free(pack_buffer);
}

// тест с раздельной передачей каждого поля структуры
void test_separate_fields(int num_particles, int num_iterations, int rank, int size, double derived_time) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    
    // инициализация данных (еще другие случайные значения)
    srand(time(NULL) + rank + 2000);
    for (int i = 0; i < num_particles; i++) {
        particles[i].id = rank * 1000 + i;
        for (int j = 0; j < 3; j++) {
            particles[i].values[j] = (double)rand() / RAND_MAX * 100.0;
        }
        sprintf(particles[i].name, "particle_%d_%d", rank, i);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        if (rank == 0) {
            for (int i = 1; i < size; i++) {
                // отправляем каждое поле каждой частицы отдельным сообщением
                for (int j = 0; j < num_particles; j++) {
                    MPI_Send(&particles[j].id, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    MPI_Send(particles[j].values, 3, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    MPI_Send(particles[j].name, 20, MPI_CHAR, i, 0, MPI_COMM_WORLD);
                }
            }
        } else {
            // получаем каждое поле каждой частицы отдельным сообщением
            for (int j = 0; j < num_particles; j++) {
                MPI_Recv(&particles[j].id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(particles[j].values, 3, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(particles[j].name, 20, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    }
    
    double end_time = MPI_Wtime();
    double separate_time = end_time - start_time;
    
    if (rank == 0) {
        printf("Separate:     %.6f seconds for %d particles, %d iterations\n",
               separate_time, num_particles, num_iterations);
        printf("Ratio (Separate/Derived): %.2f\n", separate_time / derived_time);
    }
    
    free(particles);
}

// тест с контигуозным типом данных (для сравнения)
void test_contiguous_type(int rank, int size) {
    MPI_Datatype contiguous_type;
    // создаем контигуозный тип из 5 элементов double
    MPI_Type_contiguous(5, MPI_DOUBLE, &contiguous_type);
    MPI_Type_commit(&contiguous_type);
    
    double data[5] = {1.1, 2.2, 3.3, 4.4, 5.5};
    double recv_data[5];
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    
    // многократная передача для точного измерения времени
    for (int i = 0; i < 1000; i++) {
        if (rank == 0) {
            MPI_Send(data, 1, contiguous_type, 1, 0, MPI_COMM_WORLD);
        } else if (rank == 1) {
            MPI_Recv(recv_data, 1, contiguous_type, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    double end = MPI_Wtime();
    
    if (rank == 0) {
        printf("Contiguous type (5 doubles): %.6f seconds for 1000 iterations\n", end - start);
    }
    
    MPI_Type_free(&contiguous_type);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        printf("=== Derived Data Types Comparison ===\n");
        printf("Testing three methods of sending structured data\n");
        printf("Structure: Particle { int id; double values[3]; char name[20]; }\n");
        printf("Total size per particle: %zu bytes\n", sizeof(Particle));
        printf("Processes: %d\n\n", size);
    }
    
    // различные количества частиц для тестирования масштабируемости
    int particle_counts[] = {1, 10, 100, 1000};
    int num_counts = sizeof(particle_counts) / sizeof(particle_counts[0]);
    const int NUM_ITERATIONS = 100;  // количество повторений для каждого теста
    
    // тестируем все методы для разных количеств частиц
    for (int i = 0; i < num_counts; i++) {
        int num_particles = particle_counts[i];
        
        if (rank == 0) {
            printf("\n--- Testing with %d particles ---\n", num_particles);
        }
        
        // тестируем все три метода передачи структурированных данных
        double derived_time = test_derived_type(num_particles, NUM_ITERATIONS, rank, size);
        test_packing(num_particles, NUM_ITERATIONS, rank, size, derived_time);
        test_separate_fields(num_particles, NUM_ITERATIONS, rank, size, derived_time);
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // дополнительный тест: создание и использование контигуозного типа
    if (rank == 0) {
        printf("\n=== Testing Contiguous Type ===\n");
    }
    
    test_contiguous_type(rank, size);
    
    if (rank == 0) {
        printf("\nAll derived type tests completed!\n");
    }
    
    MPI_Finalize();
    return 0;
}
