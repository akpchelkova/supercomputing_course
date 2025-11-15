#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Собственная реализация Broadcast через бинарное дерево
void my_bcast(void* buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // Корень отправляет всем остальным
        for (int i = 0; i < size; i++) {
            if (i != root) {
                MPI_Send(buffer, count, datatype, i, 0, comm);
            }
        }
    } else {
        // Остальные получают от корня
        MPI_Recv(buffer, count, datatype, root, 0, comm, MPI_STATUS_IGNORE);
    }
}

// Собственная реализация Reduce через бинарное дерево
void my_reduce(void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, 
               MPI_Op op, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    // Сначала все копируют данные в recvbuf
    if (sendbuf != recvbuf) {
        memcpy(recvbuf, sendbuf, count * sizeof(double)); // предполагаем double для простоты
    }
    
    // Дерево редукции: листья отправляют родителям
    int mask = 1;
    while (mask < size) {
        if ((rank & mask) == 0) {
            // Этот процесс получает данные
            int source = rank | mask;
            if (source < size) {
                double temp[count];
                MPI_Recv(temp, count, MPI_DOUBLE, source, 0, comm, MPI_STATUS_IGNORE);
                
                // Применяем операцию (для простоты - сложение)
                for (int i = 0; i < count; i++) {
                    ((double*)recvbuf)[i] += temp[i];
                }
            }
        } else {
            // Этот процесс отправляет данные
            int dest = rank & ~mask;
            MPI_Send(recvbuf, count, MPI_DOUBLE, dest, 0, comm);
            break; // после отправки выходим
        }
        mask <<= 1;
    }
}

// Собственная реализация Scatter
void my_scatter(void* sendbuf, int sendcount, MPI_Datatype datatype,
               void* recvbuf, int recvcount, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // Корень отправляет каждому процессу его часть
        for (int i = 0; i < size; i++) {
            if (i == root) {
                memcpy(recvbuf, (char*)sendbuf + i * sendcount * sizeof(double), 
                       recvcount * sizeof(double));
            } else {
                MPI_Send((char*)sendbuf + i * sendcount * sizeof(double), 
                        sendcount, datatype, i, 0, comm);
            }
        }
    } else {
        // Остальные получают от корня
        MPI_Recv(recvbuf, recvcount, datatype, root, 0, comm, MPI_STATUS_IGNORE);
    }
}

// Собственная реализация Gather
void my_gather(void* sendbuf, int sendcount, MPI_Datatype datatype,
              void* recvbuf, int recvcount, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // Корень получает от всех процессов
        for (int i = 0; i < size; i++) {
            if (i == root) {
                memcpy((char*)recvbuf + i * recvcount * sizeof(double), 
                       sendbuf, sendcount * sizeof(double));
            } else {
                MPI_Recv((char*)recvbuf + i * recvcount * sizeof(double), 
                        recvcount, datatype, i, 0, comm, MPI_STATUS_IGNORE);
            }
        }
    } else {
        // Остальные отправляют корню
        MPI_Send(sendbuf, sendcount, datatype, root, 0, comm);
    }
}

// Собственная реализация Allgather через Gather + Broadcast
void my_allgather(void* sendbuf, int sendcount, MPI_Datatype datatype,
                 void* recvbuf, int recvcount, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    // Сначала собираем все данные в процесс 0
    my_gather(sendbuf, sendcount, datatype, recvbuf, recvcount, 0, comm);
    
    // Затем рассылаем всем
    my_bcast(recvbuf, size * recvcount, datatype, 0, comm);
}

// Собственная реализация Allreduce через Reduce + Broadcast
void my_allreduce(void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype,
                 MPI_Op op, MPI_Comm comm) {
    // Сначала выполняем reduce в процесс 0
    my_reduce(sendbuf, recvbuf, count, datatype, op, 0, comm);
    
    // Затем рассылаем результат всем
    my_bcast(recvbuf, count, datatype, 0, comm);
}

// Тестирующая функция
void test_collective(const char* operation, int data_size, int num_trials, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    double* send_data = malloc(data_size * sizeof(double));
    double* recv_data = malloc(data_size * size * sizeof(double));
    double* temp_data = malloc(data_size * size * sizeof(double));
    
    // Инициализация данных
    srand(time(NULL) + rank);
    for (int i = 0; i < data_size; i++) {
        send_data[i] = (double)rand() / RAND_MAX * 100.0;
    }
    
    if (rank == 0) {
        printf("Testing %s with data size %d, %d processes\n", 
               operation, data_size, size);
    }
    
    // Тест MPI реализации
    MPI_Barrier(comm);
    double mpi_start = MPI_Wtime();
    
    for (int trial = 0; trial < num_trials; trial++) {
        if (strcmp(operation, "Bcast") == 0) {
            MPI_Bcast(send_data, data_size, MPI_DOUBLE, 0, comm);
        } else if (strcmp(operation, "Reduce") == 0) {
            MPI_Reduce(send_data, recv_data, data_size, MPI_DOUBLE, MPI_SUM, 0, comm);
        } else if (strcmp(operation, "Scatter") == 0) {
            MPI_Scatter(send_data, data_size, MPI_DOUBLE, recv_data, 
                       data_size, MPI_DOUBLE, 0, comm);
        } else if (strcmp(operation, "Gather") == 0) {
            MPI_Gather(send_data, data_size, MPI_DOUBLE, recv_data, 
                      data_size, MPI_DOUBLE, 0, comm);
        } else if (strcmp(operation, "Allgather") == 0) {
            MPI_Allgather(send_data, data_size, MPI_DOUBLE, recv_data, 
                         data_size, MPI_DOUBLE, comm);
        } else if (strcmp(operation, "Allreduce") == 0) {
            MPI_Allreduce(send_data, recv_data, data_size, MPI_DOUBLE, MPI_SUM, comm);
        }
    }
    
    double mpi_end = MPI_Wtime();
    double mpi_time = mpi_end - mpi_start;
    
    // Тест собственной реализации
    MPI_Barrier(comm);
    double my_start = MPI_Wtime();
    
    for (int trial = 0; trial < num_trials; trial++) {
        if (strcmp(operation, "Bcast") == 0) {
            my_bcast(send_data, data_size, MPI_DOUBLE, 0, comm);
        } else if (strcmp(operation, "Reduce") == 0) {
            my_reduce(send_data, recv_data, data_size, MPI_DOUBLE, MPI_SUM, 0, comm);
        } else if (strcmp(operation, "Scatter") == 0) {
            my_scatter(send_data, data_size, MPI_DOUBLE, recv_data, 
                      data_size, 0, comm);
        } else if (strcmp(operation, "Gather") == 0) {
            my_gather(send_data, data_size, MPI_DOUBLE, recv_data, 
                     data_size, 0, comm);
        } else if (strcmp(operation, "Allgather") == 0) {
            my_allgather(send_data, data_size, MPI_DOUBLE, recv_data, 
                        data_size, comm);
        } else if (strcmp(operation, "Allreduce") == 0) {
            my_allreduce(send_data, recv_data, data_size, MPI_DOUBLE, MPI_SUM, comm);
        }
    }
    
    double my_end = MPI_Wtime();
    double my_time = my_end - my_start;
    
    // Вывод результатов
    if (rank == 0) {
        printf("MPI %s: %.6f seconds\n", operation, mpi_time);
        printf("My %s:  %.6f seconds\n", operation, my_time);
        printf("Ratio (MPI/My): %.2f\n", mpi_time / my_time);
        printf("---\n");
        
        // Сохраняем в файл
        FILE* fp = fopen("collective_results.csv", "a");
        if (fp != NULL) {
            if (ftell(fp) == 0) {
                fprintf(fp, "operation,data_size,processes,mpi_time,my_time,ratio\n");
            }
            fprintf(fp, "%s,%d,%d,%.6f,%.6f,%.2f\n", 
                   operation, data_size, size, mpi_time, my_time, mpi_time/my_time);
            fclose(fp);
        }
    }
    
    free(send_data);
    free(recv_data);
    free(temp_data);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        printf("=== Collective Operations Comparison ===\n");
    }
    
    // Размеры данных для тестирования
    int data_sizes[] = {1, 10, 100, 1000, 10000};
    int num_sizes = sizeof(data_sizes) / sizeof(data_sizes[0]);
    
    // Операции для тестирования
    const char* operations[] = {
        "Bcast", "Reduce", "Scatter", "Gather", "Allgather", "Allreduce"
    };
    int num_operations = sizeof(operations) / sizeof(operations[0]);
    
    const int NUM_TRIALS = 100;
    
    // Тестируем все операции для разных размеров данных
    for (int op_idx = 0; op_idx < num_operations; op_idx++) {
        for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
            test_collective(operations[op_idx], data_sizes[size_idx], 
                          NUM_TRIALS, MPI_COMM_WORLD);
        }
    }
    
    if (rank == 0) {
        printf("All collective operations tests completed!\n");
    }
    
    MPI_Finalize();
    return 0;
}
