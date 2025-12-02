#!/bin/bash
# директива для slurm - название задачи для многоУЗЛОВого запуска
#SBATCH -J mpi_min_max_multi
# директива для slurm - файл вывода для многоУЗЛОВых тестов
#SBATCH -o multi_result_%j.out
# директива для slurm - файл ошибок для многоУЗЛОВых тестов
#SBATCH -e multi_result_%j.err

# запуск на разных узлах
echo "Running on single node (16 processes):"
# отправляем задание в очередь slurm: 1 узел, 16 процессов, запускаем mpi программу с размером вектора 1000000
sbatch -N 1 -n 16 --wrap="mpirun ./mpi_min_max 1000000"

echo "Running on multiple nodes (16 processes on 8 nodes):"
# отправляем задание: 8 узлов, 16 процессов (по 2 на узел), тот же размер вектора
sbatch -N 8 -n 16 --wrap="mpirun ./mpi_min_max 1000000"

echo "Running on multiple nodes (32 processes on 2 nodes):"
# отправляем задание: 2 узла, 32 процесса (по 16 на узел), тот же размер вектора
sbatch -N 2 -n 32 --wrap="mpirun ./mpi_min_max 1000000"
