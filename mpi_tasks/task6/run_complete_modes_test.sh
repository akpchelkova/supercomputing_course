#!/bin/bash
echo "=== Complete MPI Modes Comparison ==="

# Очищаем файл результатов
echo "size,processes,mode,time" > modes_complete_results.csv

SIZES="256 512 1024"
PROCESSES="4 8"
MODES="0 1 2 3"  # STANDARD, SYNCHRONOUS, READY, BUFFERED

for size in $SIZES; do
    for proc in $PROCESSES; do
        for mode in $MODES; do
            echo "Testing: size=$size, processes=$proc, mode=$mode"
            sbatch -n $proc --wrap="mpirun -np $proc ./matrix_band_all_modes $size $mode"
        done
    done
done

echo "All experiments submitted!"
