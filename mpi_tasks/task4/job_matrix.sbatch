#!/bin/bash
#SBATCH -J matrix_test
#SBATCH -o matrix_%j.out
#SBATCH -t 00:10:00
#SBATCH -n 9

module load openmpi
./compare.sh
