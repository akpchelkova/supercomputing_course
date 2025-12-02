#!/bin/sh
# директива для slurm - задаем название задачи
#SBATCH -J mpi_min_max
# директива для slurm - файл для стандартного вывода (результатов)
#SBATCH -o result_%j.out
# директива для slurm - файл для вывода ошибок
#SBATCH -e error_%j.err
# директива для slurm - запрашиваем 8 процессов
#SBATCH -n 8
# директива для slurm - запрашиваем 1 узел кластера
#SBATCH -N 1

# загружаем модуль openmpi для работы с mpi
module load openmpi
# запускаем mpi-программу min_max используя все запрошенные процессы (8)
mpirun ./min_max
