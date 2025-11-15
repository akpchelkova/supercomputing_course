#!/bin/bash
echo "=== Comparing Band vs Cannon ==="

# Только квадратные числа процессов и подходящие размеры матриц
echo "=== 4 processes ==="
echo "Band 64:" && mpirun -np 4 ./band 64 | grep "Time:"
echo "Cannon 64:" && mpirun -np 4 ./cannon 64 | grep "Time:"
echo ""
echo "Band 128:" && mpirun -np 4 ./band 128 | grep "Time:"
echo "Cannon 128:" && mpirun -np 4 ./cannon 128 | grep "Time:"
echo ""
echo "Band 256:" && mpirun -np 4 ./band 256 | grep "Time:"
echo "Cannon 256:" && mpirun -np 4 ./cannon 256 | grep "Time:"
echo ""

echo "=== 9 processes ==="
echo "Band 256:" && mpirun -np 9 ./band 256 | grep "Time:"
echo "Cannon 256:" && mpirun -np 9 ./cannon 256 | grep "Time:"
echo ""
echo "Band 512:" && mpirun -np 9 ./band 512 | grep "Time:"
echo "Cannon 512:" && mpirun -np 9 ./cannon 512 | grep "Time:"
