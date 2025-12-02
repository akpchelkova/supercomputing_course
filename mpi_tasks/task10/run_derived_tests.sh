#!/bin/bash
echo "=== Testing Derived Data Types ==="

# тестируем на разном количестве процессов: 2, 4 и 8
for processes in 2 4 8; do
    echo "Testing with $processes processes..."
    # отправляем задание в кластер для каждого количества процессов
    sbatch -n $processes --wrap="mpirun -np $processes ./derived_types"
done

echo "All derived type tests submitted!"
