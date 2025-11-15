#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Структура для тестирования
typedef struct {
    int id;
    double values[3];
    char name[20];
} Particle;

// Функция для создания производного типа данных
MPI_Datatype create_particle_type() {
    MPI_Datatype particle_type;
    
    // Блоки для структуры Particle
    int blocklengths[3] = {1, 3, 20};  // id, values[3], name[20]
    MPI_Datatype types[3] = {MPI_INT, MPI_DOUBLE, MPI_CHAR};
    
    // Смещения
    MPI_Aint displacements[3];
    Particle dummy;
    
    MPI_Get_address(&dummy.id, &displacements[0]);
    MPI_Get_address(&dummy.values[0], &displacements[1]);
    MPI_Get_address(&dummy.name[0], &displacements[2]);
    
    // Приводим к относительным смещениям
    displacements[1] -= displacements[0];
    displacements[2] -= displacements[0];
    displacements[0] = 0;
    
    // Создаем производный тип
    MPI_Type_create_struct(3, blocklengths, displacements, types, &particle_type);
    MPI_Type_commit(&particle_type);
    
    return particle_type;
}

// Объявляем функции заранее
void test_contiguous_type(int rank, int size);

// Тест с использованием производного типа
double test_derived_type(int num_particles, int num_iterations, int rank, int size) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    
    // Инициализация данных
    srand(time(NULL) + rank);
    for (int i = 0; i < num_particles; i++) {
        particles[i].id = rank * 1000 + i;
        for (int j = 0; j < 3; j++) {
            particles[i].values[j] = (double)rand() / RAND_MAX * 100.0;
        }
        sprintf(particles[i].name, "particle_%d_%d", rank, i);
    }
    
    // Создаем производный тип
    MPI_Datatype particle_type = create_particle_type();
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // Передача данных с использованием производного типа
    for (int iter = 0; iter < num_iterations; iter++) {
        if (rank == 0) {
            // Процесс 0 отправляет всем остальным
            for (int i = 1; i < size; i++) {
                MPI_Send(particles, num_particles, particle_type, i, 0, MPI_COMM_WORLD);
            }
        } else {
            // Остальные получают
            MPI_Recv(particles, num_particles, particle_type, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    double end_time = MPI_Wtime();
    double derived_time = end_time - start_time;
    
    // Освобождаем тип
    MPI_Type_free(&particle_type);
    
    if (rank == 0) {
        printf("Derived type: %.6f seconds for %d particles, %d iterations\n",
               derived_time, num_particles, num_iterations);
    }
    
    free(particles);
    return derived_time;
}

// Тест с ручной упаковкой/распаковкой
void test_packing(int num_particles, int num_iterations, int rank, int size, double derived_time) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    char* pack_buffer = malloc(num_particles * sizeof(Particle) * 2); // буфер с запасом
    
    // Инициализация данных
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
            // Упаковка и отправка
            for (int i = 1; i < size; i++) {
                int position = 0;
                MPI_Pack(&num_particles, 1, MPI_INT, pack_buffer, 
                         num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                
                for (int j = 0; j < num_particles; j++) {
                    MPI_Pack(&particles[j].id, 1, MPI_INT, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                    MPI_Pack(particles[j].values, 3, MPI_DOUBLE, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                    MPI_Pack(particles[j].name, 20, MPI_CHAR, pack_buffer,
                            num_particles * sizeof(Particle) * 2, &position, MPI_COMM_WORLD);
                }
                
                MPI_Send(pack_buffer, position, MPI_PACKED, i, 0, MPI_COMM_WORLD);
            }
        } else {
            // Получение и распаковка
            int recv_size;
            MPI_Status status;
            MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_PACKED, &recv_size);
            
            char* recv_buffer = malloc(recv_size);
            MPI_Recv(recv_buffer, recv_size, MPI_PACKED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            int position = 0;
            int received_count;
            MPI_Unpack(recv_buffer, recv_size, &position, &received_count, 1, MPI_INT, MPI_COMM_WORLD);
            
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

// Тест с раздельной передачей полей
void test_separate_fields(int num_particles, int num_iterations, int rank, int size, double derived_time) {
    Particle* particles = malloc(num_particles * sizeof(Particle));
    
    // Инициализация данных
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
                // Отправляем каждое поле отдельно
                for (int j = 0; j < num_particles; j++) {
                    MPI_Send(&particles[j].id, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    MPI_Send(particles[j].values, 3, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
                    MPI_Send(particles[j].name, 20, MPI_CHAR, i, 0, MPI_COMM_WORLD);
                }
            }
        } else {
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

// Тест с контигуозным типом
void test_contiguous_type(int rank, int size) {
    MPI_Datatype contiguous_type;
    MPI_Type_contiguous(5, MPI_DOUBLE, &contiguous_type);
    MPI_Type_commit(&contiguous_type);
    
    double data[5] = {1.1, 2.2, 3.3, 4.4, 5.5};
    double recv_data[5];
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    
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
    
    // Разные количества частиц для тестирования
    int particle_counts[] = {1, 10, 100, 1000};
    int num_counts = sizeof(particle_counts) / sizeof(particle_counts[0]);
    const int NUM_ITERATIONS = 100;
    
    for (int i = 0; i < num_counts; i++) {
        int num_particles = particle_counts[i];
        
        if (rank == 0) {
            printf("\n--- Testing with %d particles ---\n", num_particles);
        }
        
        // Тестируем все три метода
        double derived_time = test_derived_type(num_particles, NUM_ITERATIONS, rank, size);
        test_packing(num_particles, NUM_ITERATIONS, rank, size, derived_time);
        test_separate_fields(num_particles, NUM_ITERATIONS, rank, size, derived_time);
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // Дополнительный тест: создание контигуозного типа
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
