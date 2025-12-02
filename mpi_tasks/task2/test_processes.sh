#!/bin/bash
# директива для slurm - название задачи для тестирования скалярного произведения
#SBATCH -J mpi_dot_all
# директива для slurm - файл для стандартного вывода результатов
#SBATCH -o all_result_%j.out
# директива для slurm - файл для вывода ошибок
#SBATCH -e all_error_%j.err
# директива для slurm - запрашиваем 16 процессов
#SBATCH -n 16
# директива для slurm - запрашиваем 1 узел кластера
#SBATCH -N 1

# загружаем модуль openmpi для работы с mpi
module load openmpi

echo "Testing ALL process counts with different vector sizes:"

# компилируем упрощенную версию программы с оптимизацией O3
mpicc -O3 -o dot_product_simple dot_product_simple.c -lm

# тестируем разные количества процессов
for procs in 1 2 4 8 16; do
    echo "=== PROCESSES: $procs ==="
    # для каждого количества процессов тестируем разные размеры векторов
    for vec_size in 1000 10000 100000 1000000; do
        echo "Testing size: $vec_size"
        # используем srun вместо mpirun для лучшей интеграции со slurm, запускаем с указанным количеством процессов
        srun -n $procs ./dot_product_simple $vec_size
    done
    echo "---"
done
