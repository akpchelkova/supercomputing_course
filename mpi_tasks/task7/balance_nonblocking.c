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
    
    // параметры с значениями по умолчанию:
    // total_compute - общее время вычислений для одного процесса
    // bytes_per_second - количество байт коммуникаций на секунду вычислений  
    // iterations - количество итераций эксперимента
    double total_compute = 2.0;
    double bytes_per_second = 1000000;
    int iterations = 5;
    
    // читаем параметры из аргументов командной строки
    if (argc > 1) total_compute = atof(argv[1]);
    if (argc > 2) bytes_per_second = atof(argv[2]);
    if (argc > 3) iterations = atoi(argv[3]);
    
    double start_time = 0.0;
    
    // масштабируем вычисления с количеством процессов
    // каждый процесс делает total_compute/size работы
    double local_compute_time = total_compute / size;
    int local_compute_us = (int)(local_compute_time * 1000000); // переводим в микросекунды
    // вычисляем размер сообщений пропорционально времени вычислений
    int comm_size = (int)(local_compute_time * bytes_per_second);
    if (comm_size < 8) comm_size = 8; // минимальный размер сообщения
    
    // процесс 0 выводит информацию о параметрах эксперимента
    if (rank == 0) {
        printf("=== Non-blocking Balance Experiment ===\n");
        printf("Total compute: %.2f s, Bytes/sec: %.0f, Iterations: %d\n", 
               total_compute, bytes_per_second, iterations);
        printf("Processes: %d, Local compute: %d us, Comm size: %d bytes\n", 
               size, local_compute_us, comm_size);
        start_time = MPI_Wtime();
    }
    
    // буферы для неблокирующих операций
    char* send_buffer = malloc(comm_size);
    char* recv_buffer = malloc(comm_size);
    // массивы для хранения запросов неблокирующих операций
    MPI_Request send_requests[2], recv_requests[2];
    MPI_Status statuses[2]; // статусы завершения операций
    
    // инициализация буфера тестовыми данными
    for (int i = 0; i < comm_size; i++) {
        send_buffer[i] = (char)(rank + i);
    }
    
    // синхронизируем все процессы перед началом эксперимента
    MPI_Barrier(MPI_COMM_WORLD);
    
    // переменные для измерения времени вычислений и коммуникаций
    double total_comm_time = 0.0;
    double total_comp_time = 0.0;
    
    // основной цикл эксперимента
    for (int iter = 0; iter < iterations; iter++) {
        double iter_start = MPI_Wtime();
        
        // начинаем неблокирующие операции ПЕРЕД вычислениями для совмещения
        int next = (rank + 1) % size; // следующий процесс в кольце
        int prev = (rank - 1 + size) % size; // предыдущий процесс в кольце
        
        // запускаем неблокирующую отправку - функция возвращается сразу
        MPI_Isend(send_buffer, comm_size, MPI_BYTE, next, 0, MPI_COMM_WORLD, &send_requests[0]);
        // запускаем неблокирующий прием - функция возвращается сразу
        MPI_Irecv(recv_buffer, comm_size, MPI_BYTE, prev, 0, MPI_COMM_WORLD, &recv_requests[0]);
        
        // вычисления: выполняются ПАРАЛЛЕЛЬНО с коммуникациями
        double comp_start = MPI_Wtime();
        if (local_compute_us > 0) {
            usleep(local_compute_us); // эмулируем вычислительную нагрузку
        }
        double comp_end = MPI_Wtime();
        total_comp_time += (comp_end - comp_start);
        
        // ждем завершения коммуникаций после вычислений
        double comm_start = MPI_Wtime();
        MPI_Waitall(1, send_requests, statuses); // ждем завершения отправки
        MPI_Waitall(1, recv_requests, statuses); // ждем завершения приема
        double comm_end = MPI_Wtime();
        total_comm_time += (comm_end - comm_start);
        
        // обмен буферами для следующей итерации
        // полученные данные становятся данными для отправки в следующем шаге
        char* temp = send_buffer;
        send_buffer = recv_buffer;
        recv_buffer = temp;
        
        double iter_end = MPI_Wtime();
        
        // выводим информацию о первой итерации для отладки
        if (rank == 0 && iter == 0) {
            printf("Iteration %d: compute=%.4fs, comm=%.4fs, total=%.4fs\n", 
                   iter, comp_end-comp_start, comm_end-comm_start, iter_end-iter_start);
        }
    }
    
    // анализ эффективности совмещения на процессе 0
    if (rank == 0) {
        double total_time = MPI_Wtime() - start_time;
        // идеальное время если бы вычисления и коммуникации полностью совмещались
        double ideal_time = total_comp_time + total_comm_time;
        // эффективность совмещения - отношение идеального времени к реальному
        double overlap_efficiency = (ideal_time / total_time) * 100.0;
        
        printf("\n=== RESULTS ===\n");
        printf("Total time: %.4f s\n", total_time);
        printf("Total compute time: %.4f s\n", total_comp_time);
        printf("Total communication time: %.4f s\n", total_comm_time);
        printf("Ideal time (compute + comm): %.4f s\n", ideal_time);
        printf("Overlap efficiency: %.1f%%\n", overlap_efficiency);
        
        // анализ типа ограничения производительности
        if (total_comm_time > total_comp_time) {
            printf("Communication-bound: need MORE computations\n");
            printf("Suggested compute increase: %.1fx\n", total_comm_time / total_comp_time);
        } else {
            printf("Compute-bound: communications are well hidden\n");
        }
        
        // сохраняем результаты в csv файл для последующего анализа
        FILE* fp = fopen("nonblocking_results.csv", "a");
        if (fp != NULL) {
            // если файл пустой, записываем заголовок
            if (ftell(fp) == 0) {
                fprintf(fp, "processes,total_compute,bytes_per_second,compute_time,comm_time,total_time,overlap_efficiency\n");
            }
            // записываем строку с результатами
            fprintf(fp, "%d,%.2f,%.0f,%.4f,%.4f,%.4f,%.1f\n", 
                   size, total_compute, bytes_per_second, total_comp_time, total_comm_time, total_time, overlap_efficiency);
            fclose(fp);
        }
    }
    
    // освобождаем память
    free(send_buffer);
    free(recv_buffer);
    MPI_Finalize();
    return 0;
}
