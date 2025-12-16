#!/bin/bash
# директива для slurm - название задачи для финального тестирования
#SBATCH -J final_matrix_test
# директива для slurm - файл для вывода результатов
#SBATCH -o final_results_%j.out
# директива для slurm - ограничение времени выполнения 10 минут
#SBATCH -t 00:10:00

echo "=== Final Matrix Multiplication Test ==="

# тестируем гарантированно работающие комбинации размеров и процессов

echo "--- 1 process ---"
for size in 64 128 256 512; do
    echo "Size $size:"
    # тестируем ленточный алгоритм и извлекаем время выполнения
    echo -n "Band: " && mpirun -np 1 ./band $size | grep "Time:" | awk '{print $2}'
    # тестируем алгоритм Кэннона и извлекаем время выполнения
    echo -n "Cannon: " && mpirun -np 1 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 4 processes ---" 
for size in 64 128 256 512; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 4 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 4 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 9 processes ---"
# используем только размеры которые делятся на 3 (т.к. 9 процессов = 3x3 сетка)
for size in 192 384 768; do  # 192=64*3, 384=128*3, 768=256*3
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 9 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 9 ./cannon $size | grep "Time:" | awk '{print $2}'
done

echo "--- 16 processes ---"
# используем только размеры которые делятся на 4 (т.к. 16 процессов = 4x4 сетка)
for size in 256 512 1024; do
    echo "Size $size:"
    echo -n "Band: " && mpirun -np 16 ./band $size | grep "Time:" | awk '{print $2}'
    echo -n "Cannon: " && mpirun -np 16 ./cannon $size | grep "Time:" | awk '{print $2}'
done
