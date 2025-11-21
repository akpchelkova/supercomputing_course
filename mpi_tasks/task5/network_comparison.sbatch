#!/bin/bash
#SBATCH -J network_compare
#SBATCH -o network_compare_%j.out
#SBATCH -t 00:10:00

echo "=== Network Communication Comparison ==="

# Тестируем разные конфигурации для СРЕДНЕГО соотношения (N/M = 2)
echo "1. LOCAL: 8 processes on 1 node"
sbatch -N 1 -n 8 --wrap="mpirun ./balance_final 2.0 1000000 10" -o local_8.out

echo "2. DISTRIBUTED: 8 processes on 4 nodes"  
sbatch -N 4 -n 8 --wrap="mpirun ./balance_final 2.0 1000000 10" -o distributed_8.out

echo "3. LOCAL: 16 processes on 1 node"
sbatch -N 1 -n 16 --wrap="mpirun ./balance_final 2.0 1000000 5" -o local_16.out

echo "4. DISTRIBUTED: 16 processes on 8 nodes"
sbatch -N 8 -n 16 --wrap="mpirun ./balance_final 2.0 1000000 5" -o distributed_16.out

# Тестируем для НИЗКОГО соотношения (много коммуникаций)
echo "5. NETWORK HEAVY: 8 processes on 4 nodes, low ratio"
sbatch -N 4 -n 8 --wrap="mpirun ./balance_final 2.0 1000000000 5" -o network_heavy.out

echo "All network experiments submitted!"
