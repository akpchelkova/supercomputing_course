#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <math.h>

void generate_vector(double *vector, int size, int seed) {
    srand(time(NULL) + seed);
    for (int i = 0; i < size; i++) {
        vector[i] = (double)rand() / RAND_MAX * 10.0;
    }
}

double compute_local_dot_product(double *vec1, double *vec2, int local_size) {
    double local_dot = 0.0;
    for (int i = 0; i < local_size; i++) {
        local_dot += vec1[i] * vec2[i];
    }
    return local_dot;
}

void run_experiment(int vector_size, int use_processes, int world_rank, int world_size) {
    MPI_Comm comm;
    int color = (world_rank < use_processes) ? 0 : MPI_UNDEFINED;
    MPI_Comm_split(MPI_COMM_WORLD, color, world_rank, &comm);
    
    if (color == 0) {
        int rank, size;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
        
        double start_time = MPI_Wtime();
        
        // Распределение данных
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        double *local_vec1 = (double *)malloc(local_size * sizeof(double));
        double *local_vec2 = (double *)malloc(local_size * sizeof(double));
        
        if (rank == 0) {
            double *full_vec1 = (double *)malloc(vector_size * sizeof(double));
            double *full_vec2 = (double *)malloc(vector_size * sizeof(double));
            generate_vector(full_vec1, vector_size, 1);
            generate_vector(full_vec2, vector_size, 2);
            
            int current_offset = 0;
            for (int i = 0; i < size; i++) {
                int proc_size = vector_size / size + (i < remainder ? 1 : 0);
                if (i == 0) {
                    for (int j = 0; j < proc_size; j++) {
                        local_vec1[j] = full_vec1[current_offset + j];
                        local_vec2[j] = full_vec2[current_offset + j];
                    }
                } else {
                    MPI_Send(&full_vec1[current_offset], proc_size, MPI_DOUBLE, i, 0, comm);
                    MPI_Send(&full_vec2[current_offset], proc_size, MPI_DOUBLE, i, 1, comm);
                }
                current_offset += proc_size;
            }
            free(full_vec1);
            free(full_vec2);
        } else {
            MPI_Recv(local_vec1, local_size, MPI_DOUBLE, 0, 0, comm, MPI_STATUS_IGNORE);
            MPI_Recv(local_vec2, local_size, MPI_DOUBLE, 0, 1, comm, MPI_STATUS_IGNORE);
        }
        
        // Локальные вычисления
        double local_dot = compute_local_dot_product(local_vec1, local_vec2, local_size);
        
        // Глобальная редукция
        double global_dot;
        MPI_Reduce(&local_dot, &global_dot, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        
        double end_time = MPI_Wtime();
        
        if (rank == 0) {
            printf("PROCESSES: %d, SIZE: %d, TIME: %.6f\n", use_processes, vector_size, end_time - start_time);
        }
        
        free(local_vec1);
        free(local_vec2);
        MPI_Comm_free(&comm);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char *argv[]) {
    int world_rank, world_size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (world_rank == 0) {
        printf("Starting MPI Dot Product experiments...\n");
    }
    
    int processes_to_test[] = {1, 2, 4, 8, 16};
    int vector_sizes[] = {1000, 10000, 100000, 1000000};
    
    int num_proc_tests = sizeof(processes_to_test) / sizeof(processes_to_test[0]);
    int num_size_tests = sizeof(vector_sizes) / sizeof(vector_sizes[0]);
    
    for (int i = 0; i < num_proc_tests; i++) {
        for (int j = 0; j < num_size_tests; j++) {
            if (processes_to_test[i] <= world_size) {
                run_experiment(vector_sizes[j], processes_to_test[i], world_rank, world_size);
            }
        }
    }
    
    if (world_rank == 0) {
        printf("All experiments completed.\n");
    }
    
    MPI_Finalize();
    return 0;
}
