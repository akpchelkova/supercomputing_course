#!/bin/bash
echo "=== MPI Task 8: Send/Recv vs Sendrecv Comparison ==="

# компиляция обеих версий программ
echo "Compiling both versions..."
mpicc -O2 message_exchange.c -o message_exchange
mpicc -O2 message_exchange_sendrecv.c -o message_exchange_sendrecv

echo "1. Testing on same node (2 processes) - Send/Recv"
# тестируем оригинальную версию на одном узле (быстрые коммуникации)
sbatch -n 2 -N 1 -J same_node_original -o same_node_original.out --wrap="mpirun ./message_exchange"

echo "2. Testing on same node (2 processes) - Sendrecv"  
# тестируем версию с Sendrecv на одном узле
sbatch -n 2 -N 1 -J same_node_sendrecv -o same_node_sendrecv.out --wrap="mpirun ./message_exchange_sendrecv"

echo "3. Testing on different nodes (4 processes, 2 nodes) - Send/Recv"
# тестируем оригинальную версию на разных узлах (медленные коммуникации)
sbatch -n 4 -N 2 -J diff_node_original -o diff_node_original.out --wrap="mpirun ./message_exchange"

echo "4. Testing on different nodes (4 processes, 2 nodes) - Sendrecv"
# тестируем версию с Sendrecv на разных узлах
sbatch -n 4 -N 2 -J diff_node_sendrecv -o diff_node_sendrecv.out --wrap="mpirun ./message_exchange_sendrecv"

echo "All jobs submitted! Check status with: squeue -u $USER"
echo "When all jobs complete, run: cat *.out"
