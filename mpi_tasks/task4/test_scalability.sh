#!/bin/bash
#SBATCH -J scalability_test
#SBATCH -o scalability_%j.out
#SBATCH -e scalability_%j.err
#SBATCH -t 02:00:00
#SBATCH --mem=16G

echo "=== ТЕСТИРОВАНИЕ МАСШТАБИРУЕМОСТИ НА 4096x4096 ==="
echo ""

SIZE=4096  # Очень большая матрица
echo "Размер матрицы: $SIZE x $SIZE"
echo ""

echo "Процессы | Band(с) | Cannon(с) | Ускорение Band | Ускорение Cannon" > scalability.txt
echo "---------|---------|-----------|----------------|-----------------" >> scalability.txt

# Базовое время на 1 процессе
echo "Измеряем время на 1 процессе..."
BAND_BASE=$(mpirun -np 1 ./band $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')
CANNON_BASE=$(mpirun -np 1 ./cannon $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')

echo "Базовое время: Band=$BAND_BASE, Cannon=$CANNON_BASE"
echo ""

# Тестируем разное количество процессов
for PROC in 1 4 9 16 25 36 49 64; do
    echo "Тестируем $PROC процессов..."
    
    # Проверяем можно ли запустить Cannon
    SQRT=$(echo "sqrt($PROC)" | bc)
    IS_SQUARE=$(echo "$SQRT * $SQRT" | bc)
    
    # Всегда тестируем Band
    BAND_TIME=$(mpirun -np $PROC ./band $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')
    BAND_SPEEDUP=$(echo "$BAND_BASE / $BAND_TIME" | bc -l | awk '{printf "%.2f", $1}')
    
    if [ $IS_SQUARE -eq $PROC ] && [ $((SIZE % SQRT)) -eq 0 ]; then
        CANNON_TIME=$(mpirun -np $PROC ./cannon $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')
        if [ ! -z "$CANNON_TIME" ]; then
            CANNON_SPEEDUP=$(echo "$CANNON_BASE / $CANNON_TIME" | bc -l | awk '{printf "%.2f", $1}')
            echo "$PROC | $BAND_TIME | $CANNON_TIME | $BAND_SPEEDUP | $CANNON_SPEEDUP" >> scalability.txt
            echo "  Band: $BAND_TIME с (ускорение: $BAND_SPEEDUP), Cannon: $CANNON_TIME с (ускорение: $CANNON_SPEEDUP)"
        else
            echo "$PROC | $BAND_TIME | ERROR | $BAND_SPEEDUP | ERROR" >> scalability.txt
            echo "  Band: $BAND_TIME с (ускорение: $BAND_SPEEDUP), Cannon: ОШИБКА"
        fi
    else
        echo "$PROC | $BAND_TIME | SKIP | $BAND_SPEEDUP | SKIP" >> scalability.txt
        echo "  Band: $BAND_TIME с (ускорение: $BAND_SPEEDUP), Cannon: не подходит"
    fi
done

echo ""
echo "=== РЕЗУЛЬТАТЫ МАСШТАБИРУЕМОСТИ ==="
cat scalability.txt
