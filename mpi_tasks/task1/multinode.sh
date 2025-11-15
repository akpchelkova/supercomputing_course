#!/bin/bash
#SBATCH -J mpi_min_max_multi
#SBATCH -o multi_result_%j.out
#SBATCH -e multi_result_%j.err

# Запуск на разных узлах
echo "Running on single node (16 processes):"
sbatch -N 1 -n 16 --wrap="mpirun ./mpi_min_max 1000000"

echo "Running on multiple nodes (16 processes on 8 nodes):"
sbatch -N 8 -n 16 --wrap="mpirun ./mpi_min_max 1000000"

echo "Running on multiple nodes (32 processes on 2 nodes):"
sbatch -N 2 -n 32 --wrap="mpirun ./mpi_min_max 1000000"
