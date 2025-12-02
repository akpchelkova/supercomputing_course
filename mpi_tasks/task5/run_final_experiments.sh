#!/bin/bash
echo "=== FINAL Balance Experiments ==="

# очищаем файл результатов и записываем заголовок CSV
echo "processes,total_compute,bytes_per_second,ratio,compute_per_process,comm_size,parallel_time,speedup,efficiency" > balance_final_results.csv

# тестируем разные соотношения вычислений и коммуникаций (N/M)

# 1. высокое соотношение: много вычислений, мало коммуникаций
# это случай когда программа хорошо масштабируется
echo "=== HIGH Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~2000"
    # total_compute=2.0, bytes_per_second=1000, ratio ≈ 2000
    # много вычислений, мало коммуникаций - ожидаем хорошее ускорение
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000 5"
done

# 2. среднее соотношение: сбалансированный случай
echo "=== MEDIUM Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~2"
    # total_compute=2.0, bytes_per_second=1000000, ratio ≈ 2
    # сбалансированное соотношение - умеренное ускорение
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000000 5"
done

# 3. низкое соотношение: мало вычислений, много коммуникаций
# это случай когда программа плохо масштабируется
echo "=== LOW Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~0.002"
    # total_compute=2.0, bytes_per_second=1000000000, ratio ≈ 0.002
    # мало вычислений, много коммуникаций - ожидаем плохое ускорение
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000000000 5"
done

# 4. сравнение локальных vs сетевых коммуникаций
echo "=== Single vs Distributed Nodes ==="
# на одном узле (быстрые коммуникации через shared memory)
sbatch -n 8 --wrap="mpirun -np 8 ./balance_final 2.0 1000000 5"
# на разных узлах (медленные коммуникации через сеть)
sbatch -N 4 -n 8 --wrap="mpirun -np 8 ./balance_final 2.0 1000000 5"

echo "All final experiments submitted!"
