#!/bin/bash
echo "=== Testing Collective Operations ==="

# Очищаем файл результатов
echo "operation,data_size,processes,mpi_time,my_time,ratio" > collective_results.csv

# Тестируем на разном количестве процессов
for processes in 4 8 16; do
    echo "Testing with $processes processes..."
    sbatch -n $processes --wrap="mpirun -np $processes ./collective_operations"
done

echo "All collective operations tests submitted!"
