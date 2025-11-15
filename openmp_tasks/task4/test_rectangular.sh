#!/bin/bash
echo "Сравнение прямоугольных матриц (4 потока):"
echo "=========================================="

echo "--- 1000x100 ---"
OMP_NUM_THREADS=4 ./matrix_min_max 1000 100
echo ""

echo "--- 100x1000 ---"
OMP_NUM_THREADS=4 ./matrix_min_max 100 1000
echo ""

echo "--- 2000x500 ---"
OMP_NUM_THREADS=4 ./matrix_min_max 2000 500
echo ""
