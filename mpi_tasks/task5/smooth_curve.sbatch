#!/bin/bash
#SBATCH -J smooth_curve
#SBATCH -o smooth_curve_%j.out
#SBATCH -t 00:05:00

# Запускаем для 8 процессов с разными соотношениями N/M
for ratio in 0.001 0.002 0.005 0.01 0.02 0.05 0.1 0.2 0.5 1 2 5 10 20 50 100 200 500 1000 2000; do
    bytes_per_sec=$(echo "1000000 / $ratio" | bc)
    echo "Ratio: $ratio, Bytes/sec: $bytes_per_sec"
    sbatch -n 8 --wrap="mpirun ./balance_final 2.0 $bytes_per_sec 5"
done
