#!/bin/bash

echo "ЭКСПЕРИМЕНТЫ С РАЗНЫМ КОЛИЧЕСТВОМ ПОТОКОВ"
echo "=========================================="

for threads in 1 2 3 4 6 8; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./vector_sections
    echo ""
done
