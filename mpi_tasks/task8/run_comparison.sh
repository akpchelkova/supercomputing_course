#!/bin/bash
echo "=== Running Send/Recv vs Sendrecv Comparison ==="

# Оригинальная версия (Send/Recv)
echo "1. Running original Send/Recv version..."
sbatch -n 4 -N 2 -J sendrecv_original -o original_%j.out --wrap="mpirun ./message_exchange"

# Модифицированная версия (Sendrecv)  
echo "2. Running Sendrecv version..."
sbatch -n 4 -N 2 -J sendrecv_modified -o sendrecv_%j.out --wrap="mpirun ./message_exchange_sendrecv"

echo "Both jobs submitted!"
echo "Check results with: cat original_*.out && cat sendrecv_*.out"
