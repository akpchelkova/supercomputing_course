#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // получаем имя узла для диагностики распределения процессов
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    
    // диагностика распределения процессов по узлам
    if (rank == 0) {
        printf("=== MPI Message Exchange Performance ===\n");
        printf("Total processes: %d\n", size);
        
        printf("Node distribution:\n");
        printf("Process %d: %s\n", rank, processor_name);
        
        // собираем информацию о всех процессах
        for (int i = 1; i < size; i++) {
            char other_name[MPI_MAX_PROCESSOR_NAME];
            MPI_Recv(other_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Process %d: %s\n", i, other_name);
        }
        printf("\n");
    } else {
        // отправляем свое имя процессу 0
        MPI_Send(processor_name, name_len + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }
    
    // проверяем что процессов достаточно для тестирования
    if (size < 2) {
        if (rank == 0) {
            printf("ERROR: This program requires at least 2 processes!\n");
            printf("Run with: sbatch -n 2 job.sh\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    // размеры сообщений для тестирования (от 1 байта до 4 МБ)
    int message_sizes[] = {
        1,           // 1 байт
        16,          // 16 байт
        64,          // 64 байта
        256,         // 256 байт
        1024,        // 1 КБ
        4096,        // 4 КБ
        16384,       // 16 КБ
        65536,       // 64 КБ
        262144,      // 256 КБ
        1048576,     // 1 МБ
        4194304      // 4 МБ
    };
    
    int num_sizes = sizeof(message_sizes) / sizeof(message_sizes[0]);
    const int NUM_EXCHANGES = 1000; // количество обменов для усреднения
    
    if (rank == 0) {
        printf("Testing message exchange performance\n");
        printf("Exchanges per test: %d\n", NUM_EXCHANGES);
        printf("Message Size (bytes) | Time per exchange (ms) | Bandwidth (MB/s)\n");
        printf("---------------------|------------------------|------------------\n");
    }
    
    // тестируем для каждого размера сообщения
    for (int s_idx = 0; s_idx < num_sizes; s_idx++) {
        int msg_size = message_sizes[s_idx];
        char *send_buffer = (char *)malloc(msg_size * sizeof(char));
        char *recv_buffer = (char *)malloc(msg_size * sizeof(char));
        
        // инициализация буферов тестовыми данными
        for (int i = 0; i < msg_size; i++) {
            send_buffer[i] = (char)(i % 256);
        }
        
        // синхронизация перед началом замера времени
        MPI_Barrier(MPI_COMM_WORLD);
        double start_time = MPI_Wtime();
        
        // многократный обмен сообщениями между процессами 0 и 1 с использованием отдельных Send/Recv
        if (rank == 0) {
            for (int exchange = 0; exchange < NUM_EXCHANGES; exchange++) {
                // отдельная отправка сообщения процессу 1
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                // отдельный прием сообщения от процесса 1
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else if (rank == 1) {
            for (int exchange = 0; exchange < NUM_EXCHANGES; exchange++) {
                // отдельный прием сообщения от процесса 0
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // отдельная отправка сообщения процессу 0
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
            }
        }
        
        double end_time = MPI_Wtime();
        
        // проверка корректности полученных данных (только для процесса 1)
        if (rank == 1) {
            int correct = 1;
            for (int i = 0; i < msg_size; i++) {
                if (recv_buffer[i] != (char)(i % 256)) {
                    correct = 0;
                    break;
                }
            }
            if (!correct) {
                printf("WARNING: Data corruption detected for message size %d\n", msg_size);
            }
        }
        
        // процесс 0 выводит результаты
        if (rank == 0) {
            double total_time = end_time - start_time;
            double time_per_exchange = (total_time / NUM_EXCHANGES) * 1000.0; // в миллисекундах
            // вычисляем пропускную способность: общий объем данных / время
            double bandwidth = (msg_size * 2.0 * NUM_EXCHANGES) / (total_time * 1024.0 * 1024.0); // MB/s
            
            printf("%19d | %23.6f | %16.2f\n", 
                   msg_size, time_per_exchange, bandwidth);
        }
        
        free(send_buffer);
        free(recv_buffer);
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // дополнительный тест: обмен между процессами на разных узлах
    if (size >= 4 && rank == 0) {
        printf("\n=== Cross-node communication test ===\n");
        
        // ищем процесс на другом узле
        int remote_rank = -1;
        for (int i = 1; i < size; i++) {
            // простой способ: если имя узла отличается, значит на другом узле
            char other_name[MPI_MAX_PROCESSOR_NAME];
            MPI_Recv(other_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (strcmp(processor_name, other_name) != 0) {
                remote_rank = i;
                break;
            }
        }
        
        if (remote_rank != -1) {
            printf("Testing cross-node communication with process %d\n", remote_rank);
            
            int test_size = 1048576; // 1 MB
            char *test_buffer = (char *)malloc(test_size * sizeof(char));
            
            MPI_Barrier(MPI_COMM_WORLD);
            double cross_start = MPI_Wtime();
            
            // тестируем межУЗЛОВую связь с использованием отдельных Send/Recv
            for (int exchange = 0; exchange < 100; exchange++) {
                MPI_Send(test_buffer, test_size, MPI_CHAR, remote_rank, 0, MPI_COMM_WORLD);
                MPI_Recv(test_buffer, test_size, MPI_CHAR, remote_rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            
            double cross_end = MPI_Wtime();
            double cross_time = (cross_end - cross_start) / 100.0 * 1000.0;
            double cross_bandwidth = (test_size * 2.0) / (cross_time / 1000.0) / (1024.0 * 1024.0);
            
            printf("Cross-node (1MB): %.3f ms per exchange, %.2f MB/s\n", 
                   cross_time, cross_bandwidth);
            
            free(test_buffer);
        } else {
            printf("No remote node found for cross-node test\n");
        }
    } else if (rank > 0) {
        // процессы отправляют свои имена процессу 0 для проверки распределения по узлам
        MPI_Send(processor_name, name_len + 1, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        printf("\nAll message exchange tests completed!\n");
    }
    
    MPI_Finalize();
    return 0;
}
