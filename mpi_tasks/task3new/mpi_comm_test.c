#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define MAX_MESSAGE_SIZE 33554432  // 32 МБ
#define NUM_ITERATIONS 100
#define WARMUP_ITERATIONS 10

// Структура для хранения результатов
typedef struct {
    int message_size;
    int num_processes;
    double avg_time;
    double min_time;
    double max_time;
    double bandwidth;
} TestResult;

// Функция для ping-pong теста между двумя процессами
TestResult ping_pong_test(int rank, int size, int message_size, int num_exchanges) {
    TestResult result = {0};
    result.message_size = message_size;
    result.num_processes = size;
    
    char *send_buffer = NULL;
    char *recv_buffer = NULL;
    MPI_Status status;
    
    // Выделяем память
    send_buffer = (char*)malloc(message_size * sizeof(char));
    recv_buffer = (char*)malloc(message_size * sizeof(char));
    
    if (!send_buffer || !recv_buffer) {
        fprintf(stderr, "Rank %d: Failed to allocate %d bytes\n", rank, message_size);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Инициализация буферов
    srand(time(NULL) + rank);
    for (int i = 0; i < message_size; i++) {
        send_buffer[i] = (char)(rand() % 256);
    }
    memset(recv_buffer, 0, message_size);
    
    // Разогрев (warm-up)
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        if (rank == 0) {
            MPI_Send(send_buffer, message_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            MPI_Recv(recv_buffer, message_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &status);
        } else if (rank == 1) {
            MPI_Recv(recv_buffer, message_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
            MPI_Send(send_buffer, message_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
        }
    }
    
    // Основные измерения
    MPI_Barrier(MPI_COMM_WORLD);
    
    double total_time = 0.0;
    double min_time = 1e9;
    double max_time = 0.0;
    
    if (rank == 0) {
        for (int i = 0; i < num_exchanges; i++) {
            double start_time = MPI_Wtime();
            
            MPI_Send(send_buffer, message_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            MPI_Recv(recv_buffer, message_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &status);
            
            double end_time = MPI_Wtime();
            double elapsed = end_time - start_time;
            
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;
            
            // Модифицируем данные
            send_buffer[0] = (send_buffer[0] + 1) % 256;
        }
        
        result.avg_time = total_time / num_exchanges;
        result.min_time = min_time;
        result.max_time = max_time;
        
        if (result.avg_time > 0) {
            // Умножаем на 2, потому что данные идут туда и обратно
            result.bandwidth = (2.0 * message_size) / (result.avg_time * 1024.0 * 1024.0);
        }
        
    } else if (rank == 1) {
        for (int i = 0; i < num_exchanges; i++) {
            MPI_Recv(recv_buffer, message_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
            MPI_Send(send_buffer, message_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
            
            send_buffer[0] = (send_buffer[0] + 1) % 256;
        }
    }
    
    free(send_buffer);
    free(recv_buffer);
    
    return result;
}

// Функция для коллективной коммуникации (все со всеми)
TestResult all_to_all_test(int rank, int size, int message_size, int num_exchanges) {
    TestResult result = {0};
    result.message_size = message_size;
    result.num_processes = size;
    
    char *send_buffer = NULL;
    char *recv_buffer = NULL;
    
    // Каждый процесс отправляет всем
    send_buffer = (char*)malloc(message_size * sizeof(char));
    recv_buffer = (char*)malloc(message_size * sizeof(char));
    
    if (!send_buffer || !recv_buffer) {
        fprintf(stderr, "Rank %d: Allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Инициализация
    srand(time(NULL) + rank);
    for (int i = 0; i < message_size; i++) {
        send_buffer[i] = (char)(rand() % 256);
    }
    
    // Разогрев
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        MPI_Alltoall(send_buffer, message_size/size, MPI_CHAR,
                    recv_buffer, message_size/size, MPI_CHAR, MPI_COMM_WORLD);
    }
    
    // Измерения
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    for (int i = 0; i < num_exchanges; i++) {
        MPI_Alltoall(send_buffer, message_size/size, MPI_CHAR,
                    recv_buffer, message_size/size, MPI_CHAR, MPI_COMM_WORLD);
        
        // Модифицируем данные
        send_buffer[0] = (send_buffer[0] + 1) % 256;
    }
    
    double end_time = MPI_Wtime();
    
    result.avg_time = (end_time - start_time) / num_exchanges;
    result.min_time = result.avg_time;
    result.max_time = result.avg_time;
    
    if (result.avg_time > 0) {
        // Для Alltoall каждый процесс отправляет (size-1) сообщений
        result.bandwidth = ((size - 1) * message_size) / (result.avg_time * 1024.0 * 1024.0);
    }
    
    free(send_buffer);
    free(recv_buffer);
    
    return result;
}

// Функция для тестирования разных конфигураций
void run_communication_test(int rank, int size, int test_type) {
    // Разные размеры сообщений для тестирования
    int message_sizes[] = {
        1,          // 1 байт - латентность
        4,          // 4 байта
        16,         // 16 байт
        64,         // 64 байта
        256,        // 256 байт
        1024,       // 1 КБ
        4096,       // 4 КБ
        16384,      // 16 КБ
        65536,      // 64 КБ
        262144,     // 256 КБ
        1048576,    // 1 МБ
        4194304,    // 4 МБ
        8388608,    // 8 МБ
        16777216,   // 16 МБ
        33554432    // 32 МБ
    };
    
    int num_sizes = sizeof(message_sizes) / sizeof(message_sizes[0]);
    
    if (rank == 0) {
        if (test_type == 0) {
            printf("\n=== PING-PONG TEST (2 processes) ===\n");
        } else {
            printf("\n=== ALL-TO-ALL TEST (%d processes) ===\n", size);
        }
        printf("MsgSize(B)  Processes  AvgTime(ms)  MinTime(ms)  MaxTime(ms)  Bandwidth(MB/s)\n");
        printf("------------------------------------------------------------------------------\n");
    }
    
    for (int i = 0; i < num_sizes; i++) {
        int msg_size = message_sizes[i];
        int iterations = NUM_ITERATIONS;
        
        // Адаптируем количество итераций для больших сообщений
        if (msg_size >= 1048576) iterations = 50;
        if (msg_size >= 4194304) iterations = 20;
        if (msg_size >= 16777216) iterations = 10;
        if (msg_size >= 33554432) iterations = 5;
        
        TestResult result;
        
        if (test_type == 0 && size >= 2) {
            // Ping-pong тест только для первых двух процессов
            if (rank < 2) {
                result = ping_pong_test(rank, size, msg_size, iterations);
            }
        } else {
            // All-to-all тест для всех процессов
            result = all_to_all_test(rank, size, msg_size, iterations);
        }
        
        // Собираем результаты на процессе 0
        if (rank == 0) {
            double avg_ms = result.avg_time * 1000.0;
            double min_ms = result.min_time * 1000.0;
            double max_ms = result.max_time * 1000.0;
            
            printf("%10d  %9d  %11.6f  %11.6f  %11.6f  %15.2f\n",
                   msg_size, size, avg_ms, min_ms, max_ms, result.bandwidth);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

// Функция для измерения производительности при разном количестве процессов
void test_varying_processes(int rank, int size) {
    if (rank == 0) {
        printf("\n=============================================================\n");
        printf("MPI COMMUNICATION SCALING TEST\n");
        printf("Testing performance with different number of processes\n");
        printf("=============================================================\n");
    }
    
    // Фиксированные размеры сообщений для тестирования масштабируемости
    int fixed_sizes[] = {1024, 65536, 1048576, 4194304};
    int num_fixed_sizes = sizeof(fixed_sizes) / sizeof(fixed_sizes[0]);
    
    if (rank == 0) {
        printf("\n=== SCALING WITH MESSAGE SIZE 1KB ===\n");
        printf("Processes  AvgTime(ms)  Bandwidth(MB/s)  Efficiency(%%)\n");
        printf("------------------------------------------------------\n");
    }
    
    // Тестируем с размером 1KB
    int msg_size = 1024;
    TestResult result;
    
    if (size >= 2) {
        // Ping-pong тест
        if (rank < 2) {
            result = ping_pong_test(rank, size, msg_size, NUM_ITERATIONS);
        }
        
        if (rank == 0) {
            double base_time = result.avg_time;
            double base_bandwidth = result.bandwidth;
            
            printf("%9d  %11.6f  %15.2f  %13.1f\n",
                   size, result.avg_time * 1000.0, result.bandwidth, 100.0);
        }
    }
    
    // Тестируем коллективную коммуникацию
    if (rank == 0) {
        printf("\n=== ALL-TO-ALL SCALING (1MB messages) ===\n");
        printf("Processes  AvgTime(ms)  Bandwidth(MB/s)  PerProcess(MB/s)\n");
        printf("---------------------------------------------------------\n");
    }
    
    msg_size = 1048576;
    result = all_to_all_test(rank, size, msg_size, 20);
    
    if (rank == 0) {
        double per_process_bw = result.bandwidth / size;
        printf("%9d  %11.6f  %15.2f  %18.2f\n",
               size, result.avg_time * 1000.0, result.bandwidth, per_process_bw);
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        printf("=============================================================\n");
        printf("MPI COMMUNICATION PERFORMANCE TEST SUITE\n");
        printf("Running with %d processes\n", size);
        printf("=============================================================\n");
    }
    
    // Тест 1: Ping-pong (только если есть хотя бы 2 процесса)
    if (size >= 2) {
        run_communication_test(rank, size, 0);  // 0 = ping-pong
    }
    
    // Тест 2: All-to-all (для всех процессов)
    run_communication_test(rank, size, 1);  // 1 = all-to-all
    
    // Тест 3: Масштабируемость
    test_varying_processes(rank, size);
    
    // Дополнительные тесты по запросу
    if (rank == 0 && argc > 1) {
        if (strcmp(argv[1], "--detailed") == 0) {
            printf("\n=== DETAILED LATENCY MEASUREMENT ===\n");
            
            // Тест нулевых сообщений
            MPI_Status status;
            char dummy;
            double total_time = 0.0;
            int warmup = 100;
            int measurements = 1000;
            
            if (size >= 2 && rank < 2) {
                // Разогрев
                for (int i = 0; i < warmup; i++) {
                    if (rank == 0) {
                        MPI_Send(&dummy, 0, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                        MPI_Recv(&dummy, 0, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &status);
                    } else {
                        MPI_Recv(&dummy, 0, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
                        MPI_Send(&dummy, 0, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
                    }
                }
                
                // Измерения
                if (rank == 0) {
                    for (int i = 0; i < measurements; i++) {
                        double start = MPI_Wtime();
                        MPI_Send(&dummy, 0, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                        MPI_Recv(&dummy, 0, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &status);
                        double end = MPI_Wtime();
                        total_time += (end - start);
                    }
                    
                    double avg_latency = (total_time / measurements) * 1000000.0;
                    printf("Zero-byte latency: %.2f microseconds\n", avg_latency);
                }
            }
        }
    }
    
    if (rank == 0) {
        printf("\n=============================================================\n");
        printf("All tests completed successfully!\n");
        printf("=============================================================\n");
    }
    
    MPI_Finalize();
    return 0;
}
