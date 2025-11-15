#!/bin/bash
#SBATCH -J band_test
#SBATCH -o results_%j.out
#SBATCH -t 00:05:00

mkdir -p results
mpirun -np $1 ./band $2
