#!/bin/bash
# директива для slurm - название задачи для теста размеров
#SBATCH -J mpi_sizes_test
# директива для slurm - файл вывода для теста размеров
#SBATCH -o sizes_result_%j.out
# директива для slurm - файл ошибок для теста размеров
#SBATCH -e sizes_result_%j.err
# директива для slurm - запрашиваем 8 процессов
#SBATCH -n 8
# директива для slurm - запрашиваем 1 узел
#SBATCH -N 1

# загружаем модуль openmpi
module load openmpi

echo "Testing different vector sizes with 8 processes:"
# перебираем разные размеры вектора для тестирования
for size in 1000 10000 100000 1000000 5000000 10000000; do
    echo "Size: $size"
    # запускаем mpi-программу min_max_single с текущим размером вектора
    mpirun ./min_max_single $size
    echo "---"
done
