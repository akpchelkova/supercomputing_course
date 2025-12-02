#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// собственная реализация Broadcast через последовательную отправку от корня ко всем
void my_bcast(void* buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // корень отправляет данные всем остальным процессам
        for (int i = 0; i < size; i++) {
            if (i != root) {
                MPI_Send(buffer, count, datatype, i, 0, comm);
            }
        }
    } else {
        // все остальные процессы получают данные от корня
        MPI_Recv(buffer, count, datatype, root, 0, comm, MPI_STATUS_IGNORE);
    }
}

// собственная реализация Reduce через бинарное дерево
void my_reduce(void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, 
               MPI_Op op, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    // сначала все процессы копируют свои данные в буфер приема
    if (sendbuf != recvbuf) {
        memcpy(recvbuf, sendbuf, count * sizeof(double)); // предполагаем double для простоты
    }
    
    // реализация редукции через бинарное дерево
    int mask = 1;
    while (mask < size) {
        if ((rank & mask) == 0) {
            // процесс получает данные от дочерних процессов
            int source = rank | mask;
            if (source < size) {
                double temp[count];
                MPI_Recv(temp, count, MPI_DOUBLE, source, 0, comm, MPI_STATUS_IGNORE);
                
                // применяем операцию редукции (для простоты - сложение)
                for (int i = 0; i < count; i++) {
                    ((double*)recvbuf)[i] += temp[i];
                }
            }
        } else {
            // процесс отправляет данные родительскому процессу
            int dest = rank & ~mask;
            MPI_Send(recvbuf, count, MPI_DOUBLE, dest, 0, comm);
            break; // после отправки выходим из цикла
        }
        mask <<= 1; // переходим к следующему уровню дерева
    }
}

// собственная реализация Scatter
void my_scatter(void* sendbuf, int sendcount, MPI_Datatype datatype,
               void* recvbuf, int recvcount, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // корень отправляет каждому процессу его часть данных
        for (int i = 0; i < size; i++) {
            if (i == root) {
                // для себя просто копируем данные
                memcpy(recvbuf, (char*)sendbuf + i * sendcount * sizeof(double), 
                       recvcount * sizeof(double));
            } else {
                // другим процессам отправляем их часть данных
                MPI_Send((char*)sendbuf + i * sendcount * sizeof(double), 
                        sendcount, datatype, i, 0, comm);
            }
        }
    } else {
        // остальные процессы получают свою часть данных от корня
        MPI_Recv(recvbuf, recvcount, datatype, root, 0, comm, MPI_STATUS_IGNORE);
    }
}

// собственная реализация Gather
void my_gather(void* sendbuf, int sendcount, MPI_Datatype datatype,
              void* recvbuf, int recvcount, int root, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    if (rank == root) {
        // корень получает данные от всех процессов
        for (int i = 0; i < size; i++) {
            if (i == root) {
                // от себя просто копируем данные
                memcpy((char*)recvbuf + i * recvcount * sizeof(double), 
                       sendbuf, sendcount * sizeof(double));
            } else {
                // от других процессов получаем данные
                MPI_Recv((char*)recvbuf + i * recvcount * sizeof(double), 
                        recvcount, datatype, i, 0, comm, MPI_STATUS_IGNORE);
            }
        }
    } else {
        // остальные процессы отправляют свои данные корню
        MPI_Send(sendbuf, sendcount, datatype, root, 0, comm);
    }
}

// собственная реализация Allgather через комбинацию Gather + Broadcast
void my_allgather(void* sendbuf, int sendcount, MPI_Datatype datatype,
                 void* recvbuf, int recvcount, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    // сначала собираем все данные в процесс 0 через gather
    my_gather(sendbuf, sendcount, datatype, recvbuf, recvcount, 0, comm);
    
    // затем рассылаем собранные данные всем процессам через broadcast
    my_bcast(recvbuf, size * recvcount, datatype, 0, comm);
}

// собственная реализация Allreduce через комбинацию Reduce + Broadcast  
void my_allreduce(void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype,
                 MPI_Op op, MPI_Comm comm) {
    // сначала выполняем reduce в процесс 0
    my_reduce(sendbuf, recvbuf, count, datatype, op, 0, comm);
    
    // затем рассылаем результат всем процессам через broadcast
    my_bcast(recvbuf, count, datatype, 0, comm);
}

// функция для тестирования коллективных операций
void test_collective(const char* operation, int data_size, int num_trials, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    // выделяем память для данных
    double* send_data = malloc(data_size * sizeof(double));
    double* recv_data = malloc(data_size * size * sizeof(double));
    double* temp_data = malloc(data_size * size * sizeof(double));
    
    // инициализация тестовых данных случайными числами
    srand(time(NULL) + rank);
    for (int i = 0; i < data_size; i++) {
        send_data[i] = (double)rand() / RAND_MAX * 100.0;
    }
    
    if (rank == 0) {
        printf("Testing %s with data size %d, %d processes\n", 
               operation, data_size, size);
    }
    
    // тестируем стандартную MPI реализацию
    MPI_Barrier(comm); // синхронизация перед началом измерения
    double mpi_start = MPI_Wtime(); // начинаем замер времени
    
    for (int trial = 0; trial < num_trials; trial++) {
        // выполняем соответствующую MPI операцию много раз для точности измерения
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
    
    double mpi_end = MPI_Wtime(); // заканчиваем замер времени
    double mpi_time = mpi_end - mpi_start; // вычисляем общее время
    
    // тестируем собственную реализацию
    MPI_Barrier(comm); // синхронизация перед началом измерения
    double my_start = MPI_Wtime(); // начинаем замер времени
    
    for (int trial = 0; trial < num_trials; trial++) {
        // выполняем соответствующую собственную операцию много раз
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
    
    double my_end = MPI_Wtime(); // заканчиваем замер времени
    double my_time = my_end - my_start; // вычисляем общее время
    
    // вывод результатов и сохранение в файл
    if (rank == 0) {
        printf("MPI %s: %.6f seconds\n", operation, mpi_time);
        printf("My %s:  %.6f seconds\n", operation, my_time);
        printf("Ratio (MPI/My): %.2f\n", mpi_time / my_time);
        printf("---\n");
        
        // сохраняем результаты в csv файл
        FILE* fp = fopen("collective_results.csv", "a");
        if (fp != NULL) {
            fprintf(fp, "%s,%d,%d,%.6f,%.6f,%.2f\n", 
                   operation, data_size, size, mpi_time, my_time, mpi_time/my_time);
            fclose(fp);
        }
    }
    
    // освобождаем выделенную память
    free(send_data);
    free(recv_data);
    free(temp_data);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); // инициализация MPI
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        printf("=== Collective Operations Comparison ===\n");
    }
    
    // определяем размеры данных для тестирования
    int data_sizes[] = {1, 10, 100, 1000, 10000};
    int num_sizes = sizeof(data_sizes) / sizeof(data_sizes[0]);
    
    // определяем операции для тестирования
    const char* operations[] = {
        "Bcast", "Reduce", "Scatter", "Gather", "Allgather", "Allreduce"
    };
    int num_operations = sizeof(operations) / sizeof(operations[0]);
    
    const int NUM_TRIALS = 100; // количество повторений для каждого теста
    
    // тестируем все комбинации операций и размеров данных
    for (int op_idx = 0; op_idx < num_operations; op_idx++) {
        for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
            test_collective(operations[op_idx], data_sizes[size_idx], 
                          NUM_TRIALS, MPI_COMM_WORLD);
        }
    }
    
    if (rank == 0) {
        printf("All collective operations tests completed!\n");
    }
    
    MPI_Finalize(); // завершение работы с MPI
    return 0;
}
