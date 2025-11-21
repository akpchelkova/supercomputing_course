#!/bin/sh
#SBATCH -J mpi_message_exchange_advanced
#SBATCH -o result_%j.out
#SBATCH -e error_%j.err
#SBATCH -n 4
#SBATCH -N 2

module load openmpi
mpirun ./message_exchange_advanced
