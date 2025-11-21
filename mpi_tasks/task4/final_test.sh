#!/bin/bash
#SBATCH -J final_matrix_test
#SBATCH -o final_results_%j.out
#SBATCH -t 00:10:00

echo "=== Final Matrix Multiplication Test ==="

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
# Только размеры, делящиеся на 3 (т.к. 9 процессов = 3x3 grid)
for size in 192 384 768; do  # 192=64*3, 384=128*3, 768=256*3
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 9 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 9 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 16 processes ---"
# Только размеры, делящиеся на 4 (т.к. 16 процессов = 4x4 grid)
for size in 256 512 1024; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 16 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 16 ./cannon $size | grep "Time:" | awk '{print $2}'
done
