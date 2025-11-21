#!/bin/sh
#SBATCH -J mpi_message_varying
#SBATCH -o result_varying_%j.out
#SBATCH -e error_varying_%j.err

module load openmpi

# Тестируем разное количество процессов
for n in 2 4 8 16
do
    echo "=== Running with $n processes ==="
    mpirun -n $n ./message_exchange
    echo ""
done
