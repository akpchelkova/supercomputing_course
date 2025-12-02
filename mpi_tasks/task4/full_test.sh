#!/bin/bash
# директива для slurm - название задачи для полного тестирования
#SBATCH -J matrix_test
# директива для slurm - файл для вывода результатов
#SBATCH -o results_full_%j.out
# директива для slurm - ограничение времени выполнения 10 минут
#SBATCH -t 00:10:00
# директива для slurm - запрашиваем 2 узла
#SBATCH -N 2

echo "=== Full Matrix Multiplication Test ==="

# размеры матриц для тестирования
SIZES="64 128 256 512 1024"
# количество процессов (только квадратные числа для алгоритма Кэннона)
PROCS="1 4 9 16"

# тестируем все комбинации процессов и размеров матриц
for proc in $PROCS; do
    echo "--- Testing with $proc processes ---"
    
    for size in $SIZES; do
        # проверяем подходит ли размер матрицы для алгоритма Кэннона
        # для Кэннона размер матрицы должен делиться на корень из количества процессов
        if [ $proc -eq 1 ] || [ $((size % $(echo "sqrt($proc)" | bc))) -eq 0 ]; then
            echo "Size: $size"
            
            # тестируем ленточный алгоритм
            echo -n "Band: "
            mpirun -np $proc ./band $size 2>/dev/null | grep "Time:" | awk '{print $2}'
            
            # тестируем алгоритм Кэннона (только для квадратных количеств процессов)
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
