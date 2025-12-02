#!/bin/sh
# директива для slurm - задаем название задачи для скалярного произведения
#SBATCH -J mpi_dot_product
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
# запускаем mpi-программу dot_product используя все запрошенные процессы (8)
mpirun ./dot_product
