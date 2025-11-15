#!/bin/sh
#SBATCH -J mpi_dot_product
#SBATCH -o result_%j.out
#SBATCH -e error_%j.err
#SBATCH -n 8
#SBATCH -N 1

module load openmpi
mpirun ./dot_product
