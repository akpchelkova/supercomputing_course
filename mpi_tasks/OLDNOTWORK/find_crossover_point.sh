#!/bin/bash
#SBATCH -J crossover_test
#SBATCH -o crossover_%j.out
#SBATCH -e crossover_%j.err
#SBATCH -t 01:00:00
#SBATCH --mem=8G

echo "=== ПОИСК ТОЧКИ ПЕРЕЛОМА (когда Cannon становится быстрее Band) ==="
echo "Тестируем на 4 процессах..."
echo ""

# Тестируем разные размеры от 512 до 8192
for SIZE in 512 768 1024 1536 2048 3072 4096 6144 8192; do
    echo "Размер: $SIZE x $SIZE"
    
    # Запускаем Band
    BAND_OUT=$(mpirun -np 4 ./band $SIZE 2>&1)
    BAND_TIME=$(echo "$BAND_OUT" | grep "Time:" | awk '{print $2}')
    
    # Запускаем Cannon (только если размер делится на 2)
    if [ $((SIZE % 2)) -eq 0 ]; then
        CANNON_OUT=$(mpirun -np 4 ./cannon $SIZE 2>&1)
        CANNON_TIME=$(echo "$CANNON_OUT" | grep "Time:" | awk '{print $2}')
        
        if [ ! -z "$BAND_TIME" ] && [ ! -z "$CANNON_TIME" ]; then
            RATIO=$(echo "$CANNON_TIME / $BAND_TIME" | bc -l | awk '{printf "%.3f", $1}')
            
            if [ $(echo "$RATIO < 1.0" | bc -l) -eq 1 ]; then
                echo "  Cannon ВЫИГРЫВАЕТ! ($CANNON_TIME vs $BAND_TIME, отношение: $RATIO) ✓"
                echo "$SIZE $BAND_TIME $CANNON_TIME $RATIO WIN" >> crossover_results.txt
            else
                echo "  Band быстрее ($BAND_TIME vs $CANNON_TIME, отношение: $RATIO)"
                echo "$SIZE $BAND_TIME $CANNON_TIME $RATIO LOSE" >> crossover_results.txt
            fi
        fi
    else
        echo "  Пропуск Cannon: размер $SIZE не делится на 2"
    fi
    
    echo ""
done

echo "=== АНАЛИЗ РЕЗУЛЬТАТОВ ==="
echo "Размер | Band(с) | Cannon(с) | Отношение | Результат" > analysis.txt
echo "-------|---------|-----------|-----------|----------" >> analysis.txt
cat crossover_results.txt | awk '{printf "%6d | %7.3f | %9.3f | %9.3f | %s\n", $1, $2, $3, $4, $5}' >> analysis.txt

cat analysis.txt
