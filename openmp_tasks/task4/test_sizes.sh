#!/bin/bash
echo "Сравнение разных размеров матрицы (4 потока):"
echo "============================================="

for size in 100 500 1000 2000 5000; do
    echo "--- Размер: ${size}x${size} ---"
    OMP_NUM_THREADS=4 ./matrix_min_max $size $size
    echo ""
done
