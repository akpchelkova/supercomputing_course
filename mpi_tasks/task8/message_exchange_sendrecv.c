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

    // Диагностика распределения процессов по узлам
    if (rank == 0) {
        printf("=== MPI Message Exchange with Sendrecv ===\n");
        printf("Total processes: %d\n", size);

        printf("Node distribution:\n");
        printf("Process %d: %s\n", rank, processor_name);

        for (int i = 1; i < size; i++) {
            char other_name[MPI_MAX_PROCESSOR_NAME];
            MPI_Recv(other_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Process %d: %s\n", i, other_name);
        }
        printf("\n");
    } else {
        MPI_Send(processor_name, name_len + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    // Для этого эксперимента нужно минимум 2 процесса
    if (size < 2) {
        if (rank == 0) {
            printf("ERROR: This program requires at least 2 processes!\n");
            printf("Run with: sbatch -n 2 job.sh\n");
        }
        MPI_Finalize();
        return 1;
    }

    // Размеры сообщений для тестирования (в байтах)
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
    const int NUM_EXCHANGES = 1000; // Количество обменов для усреднения

    if (rank == 0) {
        printf("Testing message exchange performance with MPI_Sendrecv\n");
        printf("Exchanges per test: %d\n", NUM_EXCHANGES);
        printf("Message Size (bytes) | Time per exchange (ms) | Bandwidth (MB/s)\n");
        printf("---------------------|------------------------|------------------\n");
    }

    for (int s_idx = 0; s_idx < num_sizes; s_idx++) {
        int msg_size = message_sizes[s_idx];
        char *send_buffer = (char *)malloc(msg_size * sizeof(char));
        char *recv_buffer = (char *)malloc(msg_size * sizeof(char));

        // Инициализация буферов
        for (int i = 0; i < msg_size; i++) {
            send_buffer[i] = (char)(i % 256);
        }

        MPI_Barrier(MPI_COMM_WORLD);
        double start_time = MPI_Wtime();

        // Многократный обмен сообщениями между процессами 0 и 1 с использованием MPI_Sendrecv
        if (rank == 0) {
            for (int exchange = 0; exchange < NUM_EXCHANGES; exchange++) {
                MPI_Sendrecv(send_buffer, msg_size, MPI_CHAR, 1, 0,
                            recv_buffer, msg_size, MPI_CHAR, 1, 1,
                            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else if (rank == 1) {
            for (int exchange = 0; exchange < NUM_EXCHANGES; exchange++) {
                MPI_Sendrecv(send_buffer, msg_size, MPI_CHAR, 0, 1,
                            recv_buffer, msg_size, MPI_CHAR, 0, 0,
                            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }

        double end_time = MPI_Wtime();

        // Проверка корректности данных
        if (rank == 0 || rank == 1) {
            int correct = 1;
            for (int i = 0; i < msg_size; i++) {
                if (recv_buffer[i] != (char)(i % 256)) {
                    correct = 0;
                    break;
                }
            }
            if (!correct && rank == 0) {
                printf("WARNING: Data corruption detected for message size %d\n", msg_size);
            }
        }

        if (rank == 0) {
            double total_time = end_time - start_time;
            double time_per_exchange = (total_time / NUM_EXCHANGES) * 1000.0; // в миллисекундах
            double bandwidth = (msg_size * 2.0 * NUM_EXCHANGES) / (total_time * 1024.0 * 1024.0); // MB/s

            printf("%19d | %23.6f | %16.2f\n",
                   msg_size, time_per_exchange, bandwidth);
        }

        free(send_buffer);
        free(recv_buffer);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Дополнительный тест: обмен между процессами на разных узлах с Sendrecv
    if (size >= 4 && rank == 0) {
        printf("\n=== Cross-node communication test with Sendrecv ===\n");

        // Найдем процесс на другом узле
        int remote_rank = -1;
        for (int i = 1; i < size; i++) {
            char other_name[MPI_MAX_PROCESSOR_NAME];
            MPI_Recv(other_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (strcmp(processor_name, other_name) != 0) {
                remote_rank = i;
                break;
            }
        }

        if (remote_rank != -1) {
            printf("Testing cross-node communication with process %d using Sendrecv\n", remote_rank);

            int test_size = 1048576; // 1 MB
            char *send_buf = (char *)malloc(test_size * sizeof(char));
            char *recv_buf = (char *)malloc(test_size * sizeof(char));

            // Инициализация
            for (int i = 0; i < test_size; i++) {
                send_buf[i] = (char)(i % 256);
            }

            MPI_Barrier(MPI_COMM_WORLD);
            double cross_start = MPI_Wtime();

            for (int exchange = 0; exchange < 100; exchange++) {
                MPI_Sendrecv(send_buf, test_size, MPI_CHAR, remote_rank, 0,
                            recv_buf, test_size, MPI_CHAR, remote_rank, 1,
                            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            double cross_end = MPI_Wtime();
            double cross_time = (cross_end - cross_start) / 100.0 * 1000.0;
            double cross_bandwidth = (test_size * 2.0) / (cross_time / 1000.0) / (1024.0 * 1024.0);

            printf("Cross-node Sendrecv (1MB): %.3f ms per exchange, %.2f MB/s\n",
                   cross_time, cross_bandwidth);

            free(send_buf);
            free(recv_buf);
        } else {
            printf("No remote node found for cross-node test\n");
        }
    } else if (rank > 0) {
        MPI_Send(processor_name, name_len + 1, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
    }

    // Сравнительный тест: Send/Recv vs Sendrecv для одного размера
    if (rank == 0) {
        printf("\n=== Comparative Test: Send/Recv vs Sendrecv ===\n");
        
        int comp_size = 65536; // 64 KB
        char *buf1 = malloc(comp_size);
        char *buf2 = malloc(comp_size);
        
        // Тест Send/Recv
        MPI_Barrier(MPI_COMM_WORLD);
        double start1 = MPI_Wtime();
        for (int i = 0; i < 500; i++) {
            MPI_Send(buf1, comp_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            MPI_Recv(buf2, comp_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        double end1 = MPI_Wtime();
        
        // Тест Sendrecv
        MPI_Barrier(MPI_COMM_WORLD);
        double start2 = MPI_Wtime();
        for (int i = 0; i < 500; i++) {
            MPI_Sendrecv(buf1, comp_size, MPI_CHAR, 1, 0,
                        buf2, comp_size, MPI_CHAR, 1, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        double end2 = MPI_Wtime();
        
        printf("Send/Recv: %.4f seconds for 500 exchanges\n", end1 - start1);
        printf("Sendrecv:  %.4f seconds for 500 exchanges\n", end2 - start2);
        printf("Improvement: %.1f%%\n", ((end1 - start1) - (end2 - start2)) / (end1 - start1) * 100.0);
        
        free(buf1);
        free(buf2);
    } else if (rank == 1) {
        int comp_size = 65536;
        char *buf1 = malloc(comp_size);
        char *buf2 = malloc(comp_size);
        
        // Для Send/Recv
        for (int i = 0; i < 500; i++) {
            MPI_Recv(buf1, comp_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(buf2, comp_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
        }
        
        // Для Sendrecv
        for (int i = 0; i < 500; i++) {
            MPI_Sendrecv(buf1, comp_size, MPI_CHAR, 0, 1,
                        buf2, comp_size, MPI_CHAR, 0, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        free(buf1);
        free(buf2);
    }

    if (rank == 0) {
        printf("\nAll Sendrecv tests completed!\n");
    }

    MPI_Finalize();
    return 0;
}
