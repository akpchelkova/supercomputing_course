#!/bin/bash
echo "Эксперименты с разным количеством потоков (1M элементов):"
echo "========================================================="

for threads in 1 2 4 8 16; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./dot_product 1000000
    echo ""
done
