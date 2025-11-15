#!/bin/bash
echo "Исследование влияния количества потоков:"
echo "========================================"

for threads in 2 4 8 16; do
    echo "--- $threads потоков ---"
    ./schedule_research $threads
    echo ""
done
