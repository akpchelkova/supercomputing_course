#!/bin/bash
#SBATCH -J mpi_dot_all
#SBATCH -o all_result_%j.out
#SBATCH -e all_error_%j.err
#SBATCH -n 16
#SBATCH -N 1

module load openmpi

echo "Testing ALL process counts with different vector sizes:"

# Компилируем упрощенную версию
mpicc -O3 -o dot_product_simple dot_product_simple.c -lm

# Тестируем разные количества процессов
for procs in 1 2 4 8 16; do
    echo "=== PROCESSES: $procs ==="
    for vec_size in 1000 10000 100000 1000000; do
        echo "Testing size: $vec_size"
        # Используем srun вместо mpirun для лучшей интеграции со SLURM
        srun -n $procs ./dot_product_simple $vec_size
    done
    echo "---"
done
