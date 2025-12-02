#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    int rank, size, N = 64;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // читаем размер матрицы из аргументов командной строки если указан
    if (argc > 1) N = atoi(argv[1]);
    
    double start_time = 0;
    double *A = NULL, *B = NULL, *C = NULL;
    double *local_A, *local_C;
    
    // главный процесс создает и инициализирует матрицы
    if (rank == 0) {
        A = malloc(N * N * sizeof(double));
        B = malloc(N * N * sizeof(double));
        C = malloc(N * N * sizeof(double));
        
        // инициализируем генератор случайных чисел
        srand(time(NULL));
        // заполняем матрицы A и B случайными значениями
        for (int i = 0; i < N * N; i++) {
            A[i] = (double)rand() / RAND_MAX * 100.0;
            B[i] = (double)rand() / RAND_MAX * 100.0;
        }
        
        printf("Band Matrix Multiplication: %dx%d, %d processes\n", N, N, size);
        // начинаем замер времени выполнения
        start_time = MPI_Wtime();
    }
    
    // рассылаем размер матрицы всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // вычисляем количество строк которые будет обрабатывать каждый процесс
    int local_rows = N / size;
    // выделяем память под локальные части матриц
    local_A = malloc(local_rows * N * sizeof(double));
    local_C = malloc(local_rows * N * sizeof(double));
    
    // распределяем матрицу A по строкам между процессами
    // каждый процесс получает свой набор строк матрицы A
    MPI_Scatter(A, local_rows * N, MPI_DOUBLE, 
                local_A, local_rows * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // рассылаем всю матрицу B всем процессам
    // так как каждому процессу нужна вся матрица B для умножения
    if (rank != 0) B = malloc(N * N * sizeof(double));
    MPI_Bcast(B, N * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // локальное умножение части матрицы A на всю матрицу B
    for (int i = 0; i < local_rows; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;
            // вычисляем скалярное произведение i-й строки local_A на j-й столбец B
            for (int k = 0; k < N; k++) {
                sum += local_A[i * N + k] * B[k * N + j];
            }
            local_C[i * N + j] = sum;
        }
    }
    
    // собираем результаты от всех процессов в матрицу C на главном процессе
    MPI_Gather(local_C, local_rows * N, MPI_DOUBLE,
               C, local_rows * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // вывод результатов на главном процессе
    if (rank == 0) {
        double time = MPI_Wtime() - start_time;
        printf("Time: %.4f seconds\n", time);
        // вычисляем производительность в MFLOPS (миллионах операций с плавающей точкой в секунду)
        // количество операций: 2 * N^3 (N^3 умножений и N^3 сложений)
        printf("Performance: %.2f MFLOPS\n", (2.0 * N * N * N) / (time * 1e6));
        
        free(A); free(B); free(C);
    }
    
    // освобождаем локальную память
    free(local_A); free(local_C);
    if (rank != 0) free(B);
    
    MPI_Finalize();
    return 0;
}
