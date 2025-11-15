#!/bin/bash
echo "=== FINAL Balance Experiments ==="

# Очищаем файл результатов
echo "processes,total_compute,bytes_per_second,ratio,compute_per_process,comm_size,parallel_time,speedup,efficiency" > balance_final_results.csv

# Разные соотношения N/M (вычисления/коммуникации)

# 1. ВЫСОКОЕ соотношение: много вычислений, мало коммуникаций
echo "=== HIGH Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~2000"
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000 5"
done

# 2. СРЕДНЕЕ соотношение: сбалансировано
echo "=== MEDIUM Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~2"
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000000 5"
done

# 3. НИЗКОЕ соотношение: мало вычислений, много коммуникаций
echo "=== LOW Compute/Comm Ratio ==="
for p in 1 2 4 8 16; do
    echo "Submitting: $p processes, ratio ~0.002"
    sbatch -n $p --wrap="mpirun -np $p ./balance_final 2.0 1000000000 5"
done

# 4. Сравнение локальных vs сетевых коммуникаций
echo "=== Single vs Distributed Nodes ==="
# На одном узле
sbatch -n 8 --wrap="mpirun -np 8 ./balance_final 2.0 1000000 5"
# На разных узлах
sbatch -N 4 -n 8 --wrap="mpirun -np 8 ./balance_final 2.0 1000000 5"

echo "All final experiments submitted!"
