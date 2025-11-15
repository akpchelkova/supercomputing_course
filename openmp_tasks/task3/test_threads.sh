#!/bin/bash
echo "Эксперименты с разным количеством потоков (100M разбиений):"
echo "============================================================"

for threads in 1 2 4 8 16; do
    echo "--- $threads потоков ---"
    OMP_NUM_THREADS=$threads ./integral 100000000
    echo ""
done
