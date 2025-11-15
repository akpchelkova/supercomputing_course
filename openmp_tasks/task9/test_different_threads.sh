#!/bin/bash
echo "ЭКСПЕРИМЕНТЫ С РАЗНЫМ КОЛИЧЕСТВОМ ПОТОКОВ"
echo "========================================"

for threads in 2 4 8; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./task4_nested_comparison
    echo ""
done
