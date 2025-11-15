#!/bin/bash
echo "Сравнение разных размеров векторов (4 потока):"
echo "=============================================="

for size in 1000 10000 100000 1000000 10000000; do
    echo "--- Размер: $size ---"
    OMP_NUM_THREADS=4 ./dot_product $size
    echo ""
done
