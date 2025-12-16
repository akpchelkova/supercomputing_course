#!/bin/bash
#SBATCH -J simple_cannon_test
#SBATCH -o simple_cannon_%j.out
#SBATCH -e simple_cannon_%j.err
#SBATCH -t 00:15:00
#SBATCH --mem=4G

echo "=== ПРОСТОЙ ТЕСТ CANNON НА БОЛЬШИХ МАТРИЦАХ ==="
echo ""

echo "1. Тестируем на 4 процессах (размер должен делиться на 2):"
echo "----------------------------------------------------------"

echo "а) Маленькая матрица 512x512:"
echo "   Band:"
mpirun -np 4 ./band 512 2>&1 | grep "Time:"
echo "   Cannon:"
mpirun -np 4 ./cannon 512 2>&1 | grep "Time:"
echo ""

echo "б) Средняя матрица 1024x1024:"
echo "   Band:"
mpirun -np 4 ./band 1024 2>&1 | grep "Time:"
echo "   Cannon:"
mpirun -np 4 ./cannon 1024 2>&1 | grep "Time:"
echo ""

echo "в) Большая матрица 2048x2048:"
echo "   Band:"
mpirun -np 4 ./band 2048 2>&1 | grep "Time:"
echo "   Cannon:"
timeout 60 mpirun -np 4 ./cannon 2048 2>&1 | grep "Time:"
echo ""

echo "2. Тестируем на 16 процессах (размер должен делиться на 4):"
echo "-----------------------------------------------------------"

echo "а) Матрица 1024x1024:"
echo "   Band:"
mpirun -np 16 ./band 1024 2>&1 | grep "Time:"
echo "   Cannon:"
timeout 60 mpirun -np 16 ./cannon 1024 2>&1 | grep "Time:"
echo ""

echo "б) Матрица 2048x2048:"
echo "   Band:"
timeout 60 mpirun -np 16 ./band 2048 2>&1 | grep "Time:"
echo "   Cannon:"
timeout 120 mpirun -np 16 ./cannon 2048 2>&1 | grep "Time:"
echo ""

echo "=== СВОДКА ==="
echo ""
echo "Ожидаемые результаты:"
echo "1. На 512x512: Band быстрее"
echo "2. На 1024x1024: примерно одинаково" 
echo "3. На 2048x2048: Cannon должен быть быстрее"
echo ""
echo "Если Cannon все еще медленнее на 2048x2048:"
echo "- Увеличить размер до 4096x4096"
echo "- Или признать что на данном кластере Band эффективнее"
