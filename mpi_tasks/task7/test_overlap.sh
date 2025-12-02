#!/bin/bash
echo "=== Testing Computation-Communication Overlap ==="

# очищаем файл результатов и записываем заголовок
echo "processes,total_compute,bytes_per_second,compute_time,comm_time,total_time,overlap_efficiency" > nonblocking_results.csv

# тестируем разные соотношения вычисления/коммуникации
echo "1. High compute/comm ratio:"
# много вычислений, мало коммуникаций - ожидаем хорошее совмещение
sbatch -n 4 --wrap="mpirun -np 4 ./balance_nonblocking 2.0 1000 5"

echo "2. Balanced ratio:"
# сбалансированное соотношение - умеренное совмещение
sbatch -n 4 --wrap="mpirun -np 4 ./balance_nonblocking 2.0 1000000 5"

echo "3. High comm/compute ratio:"
# много коммуникаций, мало вычислений - ожидаем плохое совмещение
sbatch -n 4 --wrap="mpirun -np 4 ./balance_nonblocking 2.0 1000000000 5"

# тестируем на разном количестве процессов
echo "4. Different process counts:"
for p in 2 4 8 16; do
    # тестируем как масштабируется совмещение с ростом количества процессов
    sbatch -n $p --wrap="mpirun -np $p ./balance_nonblocking 2.0 1000000 5"
done

echo "All overlap experiments submitted!"
