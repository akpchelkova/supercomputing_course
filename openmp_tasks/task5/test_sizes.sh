#!/bin/bash
echo "Исследование разных размеров матриц (4 потока):"
echo "================================================"

for size in 500 1000 2000 5000; do
    echo "--- Размер: ${size}x${size} ---"
    OMP_NUM_THREADS=4 ./special_matrices $size
    echo ""
done
