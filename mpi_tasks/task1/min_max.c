#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <limits.h>

void generate_vector(double *vector, int size, int rank) {
    srand(time(NULL) + rank);
    for (int i = 0; i < size; i++) {
        vector[i] = (double)rand() / RAND_MAX * 1000.0;
    }
}

void find_local_min_max(double *vector, int local_size, double *local_min, double *local_max) {
    *local_min = vector[0];
    *local_max = vector[0];
    
    for (int i = 1; i < local_size; i++) {
        if (vector[i] < *local_min) *local_min = vector[i];
        if (vector[i] > *local_max) *local_max = vector[i];
    }
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
        
        int local_size = vector_size / size;
        int remainder = vector_size % size;
        if (rank < remainder) local_size++;
        
        double *local_vector = (double *)malloc(local_size * sizeof(double));
        
        if (rank == 0) {
            double *full_vector = (double *)malloc(vector_size * sizeof(double));
            generate_vector(full_vector, vector_size, rank);
            
            int current_offset = 0;
            for (int i = 0; i < size; i++) {
                int proc_size = vector_size / size + (i < remainder ? 1 : 0);
                if (i == 0) {
                    for (int j = 0; j < proc_size; j++) {
                        local_vector[j] = full_vector[current_offset + j];
                    }
                } else {
                    MPI_Send(&full_vector[current_offset], proc_size, MPI_DOUBLE, i, 0, comm);
                }
                current_offset += proc_size;
            }
            free(full_vector);
        } else {
            MPI_Recv(local_vector, local_size, MPI_DOUBLE, 0, 0, comm, MPI_STATUS_IGNORE);
        }
        
        double local_min, local_max;
        find_local_min_max(local_vector, local_size, &local_min, &local_max);
        
        double global_min, global_max;
        MPI_Reduce(&local_min, &global_min, 1, MPI_DOUBLE, MPI_MIN, 0, comm);
        MPI_Reduce(&local_max, &global_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        
        double end_time = MPI_Wtime();
        
        if (rank == 0) {
            printf("EXPERIMENT: processes=%d, vector_size=%d, time=%.6f\n", use_processes, vector_size, end_time - start_time);
            printf("RESULTS: min=%.2f, max=%.2f\n", global_min, global_max);
            printf("---\n");
        }
        
        free(local_vector);
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
        printf("Starting MPI experiments...\n");
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
