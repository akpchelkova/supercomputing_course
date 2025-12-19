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
    
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    
    // Сбор информации об узлах на процессе 0
    char* all_names = NULL;
    if (rank == 0) {
        all_names = malloc(size * MPI_MAX_PROCESSOR_NAME);
    }
    
    MPI_Gather(processor_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
               all_names, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("=== MPI Message Exchange (Separate Send/Recv) ===\n");
        printf("Total processes: %d\n", size);
        printf("Node distribution:\n");
        for (int i = 0; i < size; i++) {
            printf("Process %d: %s\n", i, all_names + i * MPI_MAX_PROCESSOR_NAME);
        }
        free(all_names);
        printf("\n");
    }
    
    if (size < 2) {
        if (rank == 0) {
            printf("ERROR: Need at least 2 processes!\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    // Оптимизированные размеры сообщений
    int message_sizes[] = {
        1, 8, 64, 256, 1024, 4096, 16384,
        65536, 262144, 1048576, 4194304
    };
    
    int num_sizes = sizeof(message_sizes) / sizeof(message_sizes[0]);
    const int WARMUP = 100;      // разогрев
    const int NUM_EXCHANGES = 500; // основное измерение
    
    if (rank == 0) {
        printf("Testing separate Send/Recv operations\n");
        printf("Warmup exchanges: %d, Measurement exchanges: %d\n", WARMUP, NUM_EXCHANGES);
        printf("%-20s %-20s %-20s\n", 
               "Size (bytes)", "Time (ms)", "Bandwidth (MB/s)");
        printf("-----------------------------------------------------------\n");
    }
    
    // Находим партнера для обмена
    int partner = (rank == 0) ? 1 : 0;
    
    for (int s_idx = 0; s_idx < num_sizes; s_idx++) {
        int msg_size = message_sizes[s_idx];
        char *send_buffer = (char *)malloc(msg_size);
        char *recv_buffer = (char *)malloc(msg_size);
        
        // Инициализация
        for (int i = 0; i < msg_size; i++) {
            send_buffer[i] = (char)((i + rank) % 256);
        }
        
        // Разогрев (warm-up)
        if (rank == 0) {
            for (int i = 0; i < WARMUP; i++) {
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else if (rank == 1) {
            for (int i = 0; i < WARMUP; i++) {
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
            }
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        double start_time = MPI_Wtime();
        
        // Основное измерение
        if (rank == 0) {
            for (int i = 0; i < NUM_EXCHANGES; i++) {
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else if (rank == 1) {
            for (int i = 0; i < NUM_EXCHANGES; i++) {
                MPI_Recv(recv_buffer, msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(send_buffer, msg_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
            }
        }
        
        double end_time = MPI_Wtime();
        
        // Проверка данных
        if (rank == 1) {
            int correct = 1;
            for (int i = 0; i < (msg_size < 100 ? msg_size : 100); i++) {
                if (recv_buffer[i] != (char)((i + 0) % 256)) {
                    correct = 0;
                    break;
                }
            }
            if (!correct && msg_size > 0) {
                fprintf(stderr, "Data corruption at size %d\n", msg_size);
            }
        }
        
        if (rank == 0) {
            double total_time = end_time - start_time;
            double time_per_exchange = (total_time / NUM_EXCHANGES) * 1000.0;
            double bandwidth = (msg_size * 2.0) / (total_time / NUM_EXCHANGES) / (1024.0 * 1024.0);
            
            printf("%-20d %-20.6f %-20.2f\n", 
                   msg_size, time_per_exchange, bandwidth);
        }
        
        free(send_buffer);
        free(recv_buffer);
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}
