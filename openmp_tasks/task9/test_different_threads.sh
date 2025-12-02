#!/bin/bash
echo "эксперименты с разным количеством потоков"
echo "========================================"

for threads in 2 4 8; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./task4_nested_comparison
    echo ""
done
