#!/bin/bash
#SBATCH -J mpi_proc_test
#SBATCH -o proc_result_%j.out
#SBATCH -e proc_result_%j.err
#SBATCH -n 16
#SBATCH -N 1

module load openmpi

echo "Testing different process counts with vector size 1M:"
for procs in 1 2 4 8 16; do
    echo "Processes: $procs"
    mpirun -np $procs ./min_max_single 1000000
    echo "---"
done
