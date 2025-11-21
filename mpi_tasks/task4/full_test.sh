#!/bin/bash
#SBATCH -J matrix_test
#SBATCH -o results_full_%j.out
#SBATCH -t 00:10:00
#SBATCH -N 2

echo "=== Full Matrix Multiplication Test ==="

# Размеры матриц для тестирования
SIZES="64 128 256 512 1024"
# Количество процессов (только квадратные числа для Cannon)
PROCS="1 4 9 16"

for proc in $PROCS; do
    echo "--- Testing with $proc processes ---"
    
    for size in $SIZES; do
        # Проверяем, подходит ли размер для Cannon
        if [ $proc -eq 1 ] || [ $((size % $(echo "sqrt($proc)" | bc))) -eq 0 ]; then
            echo "Size: $size"
            
            # Band algorithm
            echo -n "Band: "
            mpirun -np $proc ./band $size 2>/dev/null | grep "Time:" | awk '{print $2}'
            
            # Cannon algorithm (только для квадратных proc)
            if [ $proc -eq 1 ] || [ $(echo "sqrt($proc)" | bc)^2 -eq $proc ] 2>/dev/null; then
                echo -n "Cannon: "
                mpirun -np $proc ./cannon $size 2>/dev/null | grep "Time:" | awk '{print $2}'
            else
                echo "Cannon: SKIP (not perfect square)"
            fi
            echo ""
        else
            echo "Size $size: SKIP (not divisible by grid size)"
        fi
    done
    echo "================================"
done
