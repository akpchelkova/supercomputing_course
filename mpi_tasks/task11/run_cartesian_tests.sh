#!/bin/bash
echo "=== Testing Cartesian Grid ==="

# Тестируем на разном количестве процессов (которые можно разложить в прямоугольник)
for processes in 4 8 9 16; do
    echo "Testing with $processes processes..."
    sbatch -n $processes --wrap="mpirun -np $processes ./cartesian_grid"
done

echo "All cartesian grid tests submitted!"
