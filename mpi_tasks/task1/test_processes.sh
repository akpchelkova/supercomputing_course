#!/bin/bash
# директива для slurm - название задачи для теста процессов
#SBATCH -J mpi_proc_test
# директива для slurm - файл вывода для теста процессов
#SBATCH -o proc_result_%j.out
# директива для slurm - файл ошибок для теста процессов
#SBATCH -e proc_result_%j.err
# директива для slurm - запрашиваем 16 процессов
#SBATCH -n 16
# директива для slurm - запрашиваем 1 узел
#SBATCH -N 1

# загружаем модуль openmpi
module load openmpi

echo "Testing different process counts with vector size 1M:"
# перебираем разное количество процессов для тестирования
for procs in 1 2 4 8 16; do
    echo "Processes: $procs"
    # запускаем mpi-программу min_max_single с фиксированным размером вектора (1 млн) и разным числом процессов
    mpirun -np $procs ./min_max_single 1000000
    echo "---"
done
