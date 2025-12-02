#!/bin/bash
echo "=== MPI Task 10: Derived Data Types Testing ==="

# компиляция программы с оптимизацией
echo "Compiling derived_types.c..."
mpicc -O2 derived_types.c -o derived_types

# тестируем разные конфигурации кластера
echo "1. Testing with 2 processes (1 node)"
sbatch -n 2 -N 1 -J derived_2proc -o derived_2proc.out --wrap="mpirun ./derived_types"

echo "2. Testing with 4 processes (2 nodes)" 
sbatch -n 4 -N 2 -J derived_4proc -o derived_4proc.out --wrap="mpirun ./derived_types"

echo "3. Testing with 8 processes (4 nodes)"
sbatch -n 8 -N 4 -J derived_8proc -o derived_8proc.out --wrap="mpirun ./derived_types"

echo "All jobs submitted! Check status with: squeue -u $USER"
echo "When jobs complete, run: cat derived_*.out"
