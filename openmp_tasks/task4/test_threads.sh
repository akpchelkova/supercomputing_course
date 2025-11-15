#!/bin/bash
echo "Эксперименты с разным количеством потоков (1000x1000 матрица):"
echo "=============================================================="

for threads in 1 2 4 8 16; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./matrix_min_max 1000 1000
    echo ""
done
