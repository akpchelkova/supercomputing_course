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
    // total_compute - общее время вычислений для одного процесса (секунды)
    // bytes_per_second - количество байт коммуникаций на секунду вычислений
    // iterations - количество итераций эксперимента
    double total_compute = 2.0;      // общее время вычислений для 1 процесса (секунды)
    double bytes_per_second = 1000;  // байт коммуникаций на секунду вычислений
    int iterations = 5;
    
    // читаем параметры из аргументов командной строки если они указаны
    if (argc > 1) total_compute = atof(argv[1]);
    if (argc > 2) bytes_per_second = atof(argv[2]);
    if (argc > 3) iterations = atoi(argv[3]);
    
    double start_time, end_time;
    
    // вычисления: масштабируем с количеством процессов - каждый процесс делает total_compute/size работы
    // это эмулирует идеальное распараллеливание вычислений
    double local_compute_time = total_compute / size; // секунды вычислений на процесс
    int local_compute_us = (int)(local_compute_time * 1000000); // переводим в микросекунды для usleep
    
    // коммуникации: пропорциональны времени вычислений процесса
    // чем больше вычислений делает процесс, тем больше данных он должен обмениваться
    int comm_size = (int)(local_compute_time * bytes_per_second);
    if (comm_size < 8) comm_size = 8; // минимальный размер сообщения
    
    // процесс 0 выводит информацию о параметрах эксперимента
    if (rank == 0) {
        printf("=== BALANCE EXPERIMENT (CORRECTED) ===\n");
        printf("Total compute time (1 process): %.2f s\n", total_compute);
        printf("Bytes per second of compute: %.0f B/s\n", bytes_per_second);
        printf("Processes: %d\n", size);
        printf("Local compute time per process: %.4f s (%d us)\n", local_compute_time, local_compute_us);
        printf("Communication size per process: %d bytes\n", comm_size);
        // вычисляем соотношение вычислений к коммуникациям
        printf("Compute/Comm ratio (N/M): %.6f\n", total_compute / (bytes_per_second / 1000000.0));
        printf("Iterations: %d\n", iterations);
        
        // начинаем замер общего времени выполнения
        start_time = MPI_Wtime();
    }
    
    // выделяем буферы для коммуникаций
    char* send_buffer = malloc(comm_size);
    char* recv_buffer = malloc(comm_size);
    
    // инициализация буфера тестовыми данными
    for (int i = 0; i < comm_size; i++) {
        send_buffer[i] = (char)(rank + i);
    }
    
    // синхронизируем все процессы перед началом эксперимента
    MPI_Barrier(MPI_COMM_WORLD);
    
    // основной цикл эксперимента
    for (int iter = 0; iter < iterations; iter++) {
        // фаза вычислений (масштабируется с количеством процессов)
        // используем usleep для эмуляции вычислительной нагрузки
        if (local_compute_us > 0) {
            usleep(local_compute_us);
        }
        
        // фаза коммуникаций (пропорциональна вычислениям)
        // выполняем обмен сообщениями по кольцевой топологии
        if (size > 1 && comm_size > 0) {
            int next = (rank + 1) % size;  // следующий процесс в кольце
            int prev = (rank - 1 + size) % size;  // предыдущий процесс в кольце
            
            // одновременная отправка и прием сообщений
            // каждый процесс отправляет следующему и принимает от предыдущего
            MPI_Sendrecv(send_buffer, comm_size, MPI_BYTE, next, 0,
                        recv_buffer, comm_size, MPI_BYTE, prev, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    // расчет метрик производительности на процессе 0
    if (rank == 0) {
        end_time = MPI_Wtime();
        double parallel_time = end_time - start_time;
        
        // ускорение (speedup) - отношение последовательного времени к параллельному
        double sequential_time = total_compute * iterations; // время на 1 процессе
        double speedup = sequential_time / parallel_time;
        // эффективность - какой процент от идеального ускорения достигнут
        double efficiency = (speedup / size) * 100.0;
        
        printf("\n=== RESULTS ===\n");
        printf("Sequential time (estimated): %.4f s\n", sequential_time);
        printf("Parallel time: %.4f s\n", parallel_time);
        printf("Speedup: %.2f\n", speedup);
        printf("Efficiency: %.1f%%\n", efficiency);
        printf("================\n");
        
        // сохраняем результаты в CSV файл для последующего анализа
        FILE* fp = fopen("balance_final_results.csv", "a");
        if (fp != NULL) {
            // если файл пустой, записываем заголовок
            if (ftell(fp) == 0) {
                fprintf(fp, "processes,total_compute,bytes_per_second,ratio,compute_per_process,comm_size,parallel_time,speedup,efficiency\n");
            }
            // вычисляем соотношение вычисления/коммуникации
            double ratio = total_compute / (bytes_per_second / 1000000.0);
            // записываем строку с результатами
            fprintf(fp, "%d,%.2f,%.0f,%.6f,%.4f,%d,%.4f,%.2f,%.1f\n", 
                   size, total_compute, bytes_per_second, ratio,
                   local_compute_time, comm_size, parallel_time, speedup, efficiency);
            fclose(fp);
        }
    }
    
    // освобождаем память
    free(send_buffer);
    free(recv_buffer);
    MPI_Finalize();
    return 0;
}
