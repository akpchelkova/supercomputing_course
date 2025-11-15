#!/bin/bash
echo "Исследование разного количества потоков (2000x2000):"
echo "===================================================="

for threads in 2 4 8 16; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./special_matrices 2000
    echo ""
done
