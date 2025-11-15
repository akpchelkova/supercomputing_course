#!/bin/bash
echo "Эксперименты с разным количеством потоков:"
echo "=========================================="

for threads in 1 2 4 8 16; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./min_max 1000000
    echo ""
done
