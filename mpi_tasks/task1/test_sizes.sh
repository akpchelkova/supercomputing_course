#!/bin/bash
#SBATCH -J mpi_sizes_test
#SBATCH -o sizes_result_%j.out
#SBATCH -e sizes_result_%j.err
#SBATCH -n 8
#SBATCH -N 1

module load openmpi

echo "Testing different vector sizes with 8 processes:"
for size in 1000 10000 100000 1000000 5000000 10000000; do
    echo "Size: $size"
    mpirun ./min_max_single $size
    echo "---"
done
