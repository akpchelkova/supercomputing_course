#!/bin/bash
#SBATCH -J matrix_proper
#SBATCH -o proper_results_%j.out
#SBATCH -t 00:10:00
#SBATCH -N 3                    # 3 узла для 16 процессов
#SBATCH --ntasks-per-node=8     # 8 процессов на узел

echo "=== Proper Matrix Multiplication Test ==="

# Тестируем гарантированно работающие комбинации
echo "--- 1 process ---"
for size in 64 128 256 512; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 1 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 1 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 4 processes ---" 
for size in 64 128 256 512; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 4 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 4 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 9 processes ---"
# Только размеры, делящиеся на 3
for size in 192 384; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 9 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 9 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 16 processes ---"
# Только размеры, делящиеся на 4
for size in 256 512; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 16 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 16 ./cannon $size | grep "Time:" | awk '{print $2}'
done
