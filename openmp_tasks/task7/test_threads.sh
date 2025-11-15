#!/bin/bash
echo "Исследование влияния количества потоков:"
echo "========================================"

for threads in 2 4 8 16; do
    echo "--- $threads потоков ---"
    ./reduction_comparison $threads
    echo ""
done
