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
    double total_compute = 2.0;      // ОБЩЕЕ время вычислений для 1 процесса (секунды)
    double bytes_per_second = 1000;  // байт коммуникаций на секунду вычислений
    int iterations = 5;
    
    if (argc > 1) total_compute = atof(argv[1]);
    if (argc > 2) bytes_per_second = atof(argv[2]);
    if (argc > 3) iterations = atoi(argv[3]);
    
    double start_time, end_time;
    
    // ВЫЧИСЛЕНИЯ: масштабируем с процессами - каждый делает total_compute/size работы
    double local_compute_time = total_compute / size; // секунды
    int local_compute_us = (int)(local_compute_time * 1000000); // микросекунды
    
    // КОММУНИКАЦИИ: пропорциональны времени вычислений процесса
    int comm_size = (int)(local_compute_time * bytes_per_second);
    if (comm_size < 8) comm_size = 8; // минимальный размер
    
    if (rank == 0) {
        printf("=== BALANCE EXPERIMENT (CORRECTED) ===\n");
        printf("Total compute time (1 process): %.2f s\n", total_compute);
        printf("Bytes per second of compute: %.0f B/s\n", bytes_per_second);
        printf("Processes: %d\n", size);
        printf("Local compute time per process: %.4f s (%d us)\n", local_compute_time, local_compute_us);
        printf("Communication size per process: %d bytes\n", comm_size);
        printf("Compute/Comm ratio (N/M): %.6f\n", total_compute / (bytes_per_second / 1000000.0));
        printf("Iterations: %d\n", iterations);
        
        start_time = MPI_Wtime();
    }
    
    // Выделяем буферы для коммуникаций
    char* send_buffer = malloc(comm_size);
    char* recv_buffer = malloc(comm_size);
    
    // Инициализация буфера
    for (int i = 0; i < comm_size; i++) {
        send_buffer[i] = (char)(rank + i);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Основной цикл
    for (int iter = 0; iter < iterations; iter++) {
        // Фаза вычислений (масштабируется с количеством процессов)
        if (local_compute_us > 0) {
            usleep(local_compute_us);
        }
        
        // Фаза коммуникаций (пропорциональна вычислениям)
        if (size > 1 && comm_size > 0) {
            int next = (rank + 1) % size;
            int prev = (rank - 1 + size) % size;
            
            MPI_Sendrecv(send_buffer, comm_size, MPI_BYTE, next, 0,
                        recv_buffer, comm_size, MPI_BYTE, prev, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    // Расчет ускорения и эффективности
    if (rank == 0) {
        end_time = MPI_Wtime();
        double parallel_time = end_time - start_time;
        
        // Ускорение (Speedup)
        double sequential_time = total_compute * iterations; // время на 1 процессе
        double speedup = sequential_time / parallel_time;
        double efficiency = (speedup / size) * 100.0;
        
        printf("\n=== RESULTS ===\n");
        printf("Sequential time (estimated): %.4f s\n", sequential_time);
        printf("Parallel time: %.4f s\n", parallel_time);
        printf("Speedup: %.2f\n", speedup);
        printf("Efficiency: %.1f%%\n", efficiency);
        printf("================\n");
        
        // Сохраняем в CSV для анализа
        FILE* fp = fopen("balance_final_results.csv", "a");
        if (fp != NULL) {
            if (ftell(fp) == 0) {
                fprintf(fp, "processes,total_compute,bytes_per_second,ratio,compute_per_process,comm_size,parallel_time,speedup,efficiency\n");
            }
            double ratio = total_compute / (bytes_per_second / 1000000.0);
            fprintf(fp, "%d,%.2f,%.0f,%.6f,%.4f,%d,%.4f,%.2f,%.1f\n", 
                   size, total_compute, bytes_per_second, ratio,
                   local_compute_time, comm_size, parallel_time, speedup, efficiency);
            fclose(fp);
        }
    }
    
    free(send_buffer);
    free(recv_buffer);
    MPI_Finalize();
    return 0;
}
