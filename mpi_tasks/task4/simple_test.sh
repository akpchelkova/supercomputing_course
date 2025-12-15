#!/bin/bash
#SBATCH -J simple_matrix_test
#SBATCH -o simple_%j.out
#SBATCH -e simple_%j.err
#SBATCH -t 00:10:00
#SBATCH -n 4

echo "=== ПРОСТОЙ ТЕСТ ==="
echo ""

echo "1. Band на 1024x1024:"
mpirun ./band 1024
echo ""

echo "2. Cannon на 1024x1024:"
mpirun ./cannon 1024
echo ""

echo "3. Band на 2048x2048:"
mpirun ./band 2048
echo ""

echo "4. Cannon на 2048x2048:"
mpirun ./cannon 2048
