#!/bin/bash
echo "=== Testing BUFFERED Mode Only ==="

# компилируем программу с оптимизацией
mpicc -O2 matrix_buffered_only.c -o matrix_buffered

# очищаем файл результатов и записываем заголовок
echo "size,processes,mode,time" > buffered_results.csv

# тестируем только буферизованный режим с разными параметрами
for size in 256 512 1024; do
    for proc in 4 8; do
        echo "Testing BUFFERED: size=$size, processes=$proc"
        # отправляем задание для тестирования буферизованного режима
        sbatch -n $proc --wrap="mpirun -np $proc ./matrix_buffered $size"
    done
done

echo "BUFFERED tests submitted!"
