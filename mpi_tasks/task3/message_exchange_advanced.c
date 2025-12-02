#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

// функция тестирования синхронного режима передачи (synchronous send)
void test_sync_mode(int rank, int partner, int msg_size, int num_exchanges) {
    // выделяем память под буферы для отправки и приема
    char *send_buf = malloc(msg_size);
    char *recv_buf = malloc(msg_size);
    
    // синхронизируем все процессы перед началом теста
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    
    // выполняем многократный обмен сообщениями
    for (int i = 0; i < num_exchanges; i++) {
        if (rank == 0) {
            // синхронная отправка - блокируется пока получатель не начнет прием
            MPI_Ssend(send_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD);
            // прием ответного сообщения
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            // прием сообщения от партнера
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // синхронная отправка ответа
            MPI_Ssend(send_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD);
        }
    }
    
    double end = MPI_Wtime();
    if (rank == 0) {
        // вычисляем среднее время на один обмен в миллисекундах
        double time_per_exchange = (end - start) / num_exchanges * 1000.0;
        printf("Sync mode: %d bytes - %.6f ms/exchange\n", msg_size, time_per_exchange);
    }
    
    free(send_buf);
    free(recv_buf);
}

// функция тестирования режима готовности (ready send)
void test_ready_mode(int rank, int partner, int msg_size, int num_exchanges) {
    char *send_buf = malloc(msg_size);
    char *recv_buf = malloc(msg_size);
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    
    for (int i = 0; i < num_exchanges; i++) {
        if (rank == 0) {
            // готовностная отправка - предполагает что получатель уже ждет сообщение
            MPI_Rsend(send_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD);
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Rsend(send_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD);
        }
    }
    
    double end = MPI_Wtime();
    if (rank == 0) {
        double time_per_exchange = (end - start) / num_exchanges * 1000.0;
        printf("Ready mode: %d bytes - %.6f ms/exchange\n", msg_size, time_per_exchange);
    }
    
    free(send_buf);
    free(recv_buf);
}

// функция тестирования буферизованного режима (buffered send)
void test_buffered_mode(int rank, int partner, int msg_size, int num_exchanges) {
    char *send_buf = malloc(msg_size);
    char *recv_buf = malloc(msg_size);
    
    // настраиваем буферизацию для буферизованной отправки
    int buffer_size = msg_size + MPI_BSEND_OVERHEAD;
    char *buffer = malloc(buffer_size);
    MPI_Buffer_attach(buffer, buffer_size);
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    
    for (int i = 0; i < num_exchanges; i++) {
        if (rank == 0) {
            // буферизованная отправка - данные копируются в буфер и функция возвращается сразу
            MPI_Bsend(send_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD);
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            MPI_Recv(recv_buf, msg_size, MPI_CHAR, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Bsend(send_buf, msg_size, MPI_CHAR, partner, 1, MPI_COMM_WORLD);
        }
    }
    
    double end = MPI_Wtime();
    // отключаем буферизацию
    MPI_Buffer_detach(&buffer, &buffer_size);
    
    if (rank == 0) {
        double time_per_exchange = (end - start) / num_exchanges * 1000.0;
        printf("Buffered mode: %d bytes - %.6f ms/exchange\n", msg_size, time_per_exchange);
    }
    
    free(send_buf);
    free(recv_buf);
    free(buffer);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // проверяем что процессов достаточно для тестирования
    if (size < 2) {
        if (rank == 0) printf("Need at least 2 processes\n");
        MPI_Finalize();
        return 1;
    }
    
    // определяем партнера для обмена (процесс 0 обменивается с процессом 1)
    int partner = (rank == 0) ? 1 : 0;
    // массив размеров сообщений для тестирования
    int test_sizes[] = {64, 1024, 4096, 16384, 65536};
    int num_sizes = 5;
    int num_exchanges = 1000; // количество обменов для усреднения результатов
    
    if (rank == 0) {
        printf("=== Testing different MPI communication modes ===\n");
        printf("Exchanges per test: %d\n\n", num_exchanges);
    }
    
    // тестируем все режимы для каждого размера сообщения
    for (int i = 0; i < num_sizes; i++) {
        int msg_size = test_sizes[i];
        if (rank == 0) printf("--- Message size: %d bytes ---\n", msg_size);
        
        // тестируем три разных режима передачи
        test_sync_mode(rank, partner, msg_size, num_exchanges);
        test_ready_mode(rank, partner, msg_size, num_exchanges);
        test_buffered_mode(rank, partner, msg_size, num_exchanges);
        
        if (rank == 0) printf("\n");
    }
    
    MPI_Finalize();
    return 0;
}
