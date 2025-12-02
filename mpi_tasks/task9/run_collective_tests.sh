#!/bin/bash
echo "=== Testing Collective Operations ==="

# очищаем файл результатов и записываем заголовок csv
echo "operation,data_size,processes,mpi_time,my_time,ratio" > collective_results.csv

# тестируем на разном количестве процессов: 4, 8 и 16
for processes in 4 8 16; do
    echo "Testing with $processes processes..."
    # отправляем задание в кластер через sbatch для каждого количества процессов
    sbatch -n $processes --wrap="mpirun -np $processes ./collective_operations"
done

echo "All collective operations tests submitted!"
