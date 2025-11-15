#!/bin/sh
#SBATCH -J mpi_min_max
#SBATCH -o result_%j.out
#SBATCH -e error_%j.err
#SBATCH -n 8              # 8 процессов
#SBATCH -N 1              # 1 узел

module load openmpi
mpirun ./min_max
