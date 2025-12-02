#!/bin/bash
echo "=== Testing Cartesian Grid ==="

# тестируем на разном количестве процессов, которые можно разложить в прямоугольную решетку
for processes in 4 8 9 16; do
    echo "Testing with $processes processes..."
    # отправляем задание в кластер для каждого количества процессов
    sbatch -n $processes --wrap="mpirun -np $processes ./cartesian_grid"
done

echo "All cartesian grid tests submitted!"
