#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Параметры: total_compute_seconds, bytes_per_second, iterations
    double total_compute = 2.0;
    double bytes_per_second = 1000000;
    int iterations = 5;
    
    if (argc > 1) total_compute = atof(argv[1]);
    if (argc > 2) bytes_per_second = atof(argv[2]);
    if (argc > 3) iterations = atoi(argv[3]);
    
    double start_time = 0.0;
    
    // Масштабируем вычисления
    double local_compute_time = total_compute / size;
    int local_compute_us = (int)(local_compute_time * 1000000);
    int comm_size = (int)(local_compute_time * bytes_per_second);
    if (comm_size < 8) comm_size = 8;
    
    if (rank == 0) {
        printf("=== Non-blocking Balance Experiment ===\n");
        printf("Total compute: %.2f s, Bytes/sec: %.0f, Iterations: %d\n", 
               total_compute, bytes_per_second, iterations);
        printf("Processes: %d, Local compute: %d us, Comm size: %d bytes\n", 
               size, local_compute_us, comm_size);
        start_time = MPI_Wtime();
    }
    
    // Буферы для неблокирующих операций
    char* send_buffer = malloc(comm_size);
    char* recv_buffer = malloc(comm_size);
    MPI_Request send_requests[2], recv_requests[2];
    MPI_Status statuses[2];
    
    // Инициализация буфера
    for (int i = 0; i < comm_size; i++) {
        send_buffer[i] = (char)(rank + i);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    double total_comm_time = 0.0;
    double total_comp_time = 0.0;
    
    for (int iter = 0; iter < iterations; iter++) {
        double iter_start = MPI_Wtime();
        
        // НАЧАЛО: Запускаем неблокирующие операции ПЕРЕД вычислениями
        int next = (rank + 1) % size;
        int prev = (rank - 1 + size) % size;
        
        // Неблокирующая отправка и прием
        MPI_Isend(send_buffer, comm_size, MPI_BYTE, next, 0, MPI_COMM_WORLD, &send_requests[0]);
        MPI_Irecv(recv_buffer, comm_size, MPI_BYTE, prev, 0, MPI_COMM_WORLD, &recv_requests[0]);
        
        // ВЫЧИСЛЕНИЯ: выполняются ПАРАЛЛЕЛЬНО с коммуникациями
        double comp_start = MPI_Wtime();
        if (local_compute_us > 0) {
            usleep(local_compute_us);
        }
        double comp_end = MPI_Wtime();
        total_comp_time += (comp_end - comp_start);
        
        // Ждем завершения коммуникаций
        double comm_start = MPI_Wtime();
        MPI_Waitall(1, send_requests, statuses);
        MPI_Waitall(1, recv_requests, statuses);
        double comm_end = MPI_Wtime();
        total_comm_time += (comm_end - comm_start);
        
        // Обмен буферами для следующей итерации
        char* temp = send_buffer;
        send_buffer = recv_buffer;
        recv_buffer = temp;
        
        double iter_end = MPI_Wtime();
        
        if (rank == 0 && iter == 0) {
            printf("Iteration %d: compute=%.4fs, comm=%.4fs, total=%.4fs\n", 
                   iter, comp_end-comp_start, comm_end-comm_start, iter_end-iter_start);
        }
    }
    
    // Анализ совмещения
    if (rank == 0) {
        double total_time = MPI_Wtime() - start_time;
        double ideal_time = total_comp_time + total_comm_time;
        double overlap_efficiency = (ideal_time / total_time) * 100.0;
        
        printf("\n=== RESULTS ===\n");
        printf("Total time: %.4f s\n", total_time);
        printf("Total compute time: %.4f s\n", total_comp_time);
        printf("Total communication time: %.4f s\n", total_comm_time);
        printf("Ideal time (compute + comm): %.4f s\n", ideal_time);
        printf("Overlap efficiency: %.1f%%\n", overlap_efficiency);
        
        // Анализ совмещения
        if (total_comm_time > total_comp_time) {
            printf("Communication-bound: need MORE computations\n");
            printf("Suggested compute increase: %.1fx\n", total_comm_time / total_comp_time);
        } else {
            printf("Compute-bound: communications are well hidden\n");
        }
        
        // Сохраняем результаты
        FILE* fp = fopen("nonblocking_results.csv", "a");
        if (fp != NULL) {
            if (ftell(fp) == 0) {
                fprintf(fp, "processes,total_compute,bytes_per_second,compute_time,comm_time,total_time,overlap_efficiency\n");
            }
            fprintf(fp, "%d,%.2f,%.0f,%.4f,%.4f,%.4f,%.1f\n", 
                   size, total_compute, bytes_per_second, total_comp_time, total_comm_time, total_time, overlap_efficiency);
            fclose(fp);
        }
    }
    
    free(send_buffer);
    free(recv_buffer);
    MPI_Finalize();
    return 0;
}
