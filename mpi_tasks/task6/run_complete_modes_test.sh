#!/bin/bash
echo "=== Complete MPI Modes Comparison ==="

# очищаем файл результатов и записываем заголовок
echo "size,processes,mode,time" > modes_complete_results.csv

# определяем параметры для тестирования
SIZES="256 512 1024"
PROCESSES="4 8"
MODES="0 1 2 3"  # STANDARD, SYNCHRONOUS, READY, BUFFERED

# запускаем все комбинации тестов
for size in $SIZES; do
    for proc in $PROCESSES; do
        for mode in $MODES; do
            echo "Testing: size=$size, processes=$proc, mode=$mode"
            # отправляем задание в slurm с указанными параметрами
            sbatch -n $proc --wrap="mpirun -np $proc ./matrix_band_all_modes $size $mode"
        done
    done
done

echo "All experiments submitted!"
