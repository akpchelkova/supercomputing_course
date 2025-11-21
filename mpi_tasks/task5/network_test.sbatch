#!/bin/bash
#SBATCH -J network_balance
#SBATCH -o network_results_%j.out
#SBATCH -t 00:05:00

# Тестируем разные конфигурации узлов
echo "=== LOCAL: 8 processes on 1 node ==="
sbatch -N 1 -n 8 --wrap="mpirun ./balance_final 2.0 1000000 10"

echo "=== DISTRIBUTED: 8 processes on 4 nodes ==="  
sbatch -N 4 -n 8 --wrap="mpirun ./balance_final 2.0 1000000 10"

echo "=== EXTREME NETWORK: 16 processes on 8 nodes ==="
sbatch -N 8 -n 16 --wrap="mpirun ./balance_final 2.0 1000000000 5"
