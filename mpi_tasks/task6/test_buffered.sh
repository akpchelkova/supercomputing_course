#!/bin/bash
echo "=== Testing BUFFERED Mode Only ==="

# Компилируем
mpicc -O2 matrix_buffered_only.c -o matrix_buffered

echo "size,processes,mode,time" > buffered_results.csv

# Тестируем BUFFERED режим
for size in 256 512 1024; do
    for proc in 4 8; do
        echo "Testing BUFFERED: size=$size, processes=$proc"
        sbatch -n $proc --wrap="mpirun -np $proc ./matrix_buffered $size"
    done
done

echo "BUFFERED tests submitted!"
