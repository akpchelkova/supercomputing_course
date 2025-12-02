#!/bin/sh
# директива для slurm - название задачи для оригинальной версии
#SBATCH -J mpi_message_exchange
# директива для slurm - файл для стандартного вывода результатов
#SBATCH -o result_%j.out
# директива для slurm - файл для вывода ошибок
#SBATCH -e error_%j.err
# директива для slurm - запрашиваем 4 процесса
#SBATCH -n 4
# директива для slurm - запрашиваем 2 узла кластера
#SBATCH -N 2

# загружаем модуль openmpi для работы с mpi
module load openmpi
# запускаем оригинальную версию программы с использованием отдельных Send/Recv
mpirun ./message_exchange
