#!/bin/bash
echo "=== Testing Derived Data Types ==="

# Тестируем на разном количестве процессов
for processes in 2 4 8; do
    echo "Testing with $processes processes..."
    sbatch -n $processes --wrap="mpirun -np $processes ./derived_types"
done

echo "All derived type tests submitted!"
