#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    double start_time, end_time;
    MPI_Comm temp_comm;
    
    if (rank == 0) {
        printf("\n=== MPI Topology Test ===\n");
        printf("Total processes: %d\n", size);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);  // синхронизация всех процессов
    
    // 1. декартова топология (2D решетка)
    if (rank == 0) printf("1. Testing Cartesian topology...\n");
    start_time = MPI_Wtime();  // начинаем замер времени
    
    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims);  // автоматически определяем размеры решетки
    int periods[2] = {0, 0};  // непериодические границы
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &temp_comm);  // создаем декартову топологию
    
    if (temp_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&temp_comm);  // освобождаем коммуникатор
    }
    end_time = MPI_Wtime();
    if (rank == 0) printf("   Cartesian time: %.6f sec\n", end_time - start_time);
    
    // 2. топология тора (периодическая декартова решетка)
    if (rank == 0) printf("2. Testing Torus topology...\n");
    start_time = MPI_Wtime();
    
    int torus_periods[2] = {1, 1};  // периодические границы по обеим осям
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, torus_periods, 1, &temp_comm);  // создаем тор
    
    if (temp_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&temp_comm);
    }
    end_time = MPI_Wtime();
    if (rank == 0) printf("   Torus time: %.6f sec\n", end_time - start_time);
    
    // 3. топология графа (произвольная связность, требуется минимум 3 процесса)
    if (size >= 3) {
        if (rank == 0) printf("3. Testing Graph topology...\n");
        start_time = MPI_Wtime();
        
        MPI_Comm graph_comm;
        int index[3];  // массив индексов для каждого узла
        int edges[6];  // массив ребер графа
        
        // создаем простой линейный граф: 0-1-2
        index[0] = 1;  // узел 0 имеет 1 соседа
        index[1] = 2;  // узел 1 имеет 2 соседа  
        index[2] = 3;  // узел 2 имеет 1 соседа
        edges[0] = 1;  // 0->1
        edges[1] = 0;  // 1->0
        edges[2] = 2;  // 1->2
        edges[3] = 1;  // 2->1
        
        MPI_Graph_create(MPI_COMM_WORLD, 3, index, edges, 0, &graph_comm);  // создаем графовую топологию
        
        if (graph_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&graph_comm);
        }
        end_time = MPI_Wtime();
        if (rank == 0) printf("   Graph time: %.6f sec\n", end_time - start_time);
    } else {
        if (rank == 0) printf("3. Graph topology: skipped (need >= 3 processes)\n");
    }
    
    // 4. топология звезды (центральный узел соединен со всеми остальными)
    if (rank == 0) printf("4. Testing Star topology...\n");
    start_time = MPI_Wtime();
    
    if (size >= 2) {
        MPI_Comm star_comm;
        int index[size];        // массив индексов для каждого узла
        int edges[2 * (size - 1)];  // массив ребер (в обе стороны)
        int i;
        
        // центральный узел 0 соединен со всеми остальными
        index[0] = size - 1;  // у центрального узла (size-1) соседей
        for (i = 1; i < size; i++) {
            index[i] = index[i-1] + 1;  // каждый следующий узел имеет на 1 соседа больше
        }
        
        // создание ребер: двусторонние связи между центром и периферийными узлами
        for (i = 0; i < size - 1; i++) {
            edges[i] = i + 1;           // из центра 0 в периферийные узлы
            edges[size - 1 + i] = 0;    // из периферийных узлов в центр 0
        }
        
        MPI_Graph_create(MPI_COMM_WORLD, size, index, edges, 1, &star_comm);  // создаем звездную топологию
        
        if (star_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&star_comm);
        }
    }
    end_time = MPI_Wtime();
    if (rank == 0) printf("   Star time: %.6f sec\n", end_time - start_time);
    
    if (rank == 0) {
        printf("=== Test Complete ===\n");
    }
    
    MPI_Finalize();
    return 0;
}
