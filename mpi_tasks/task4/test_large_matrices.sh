#!/bin/bash
#SBATCH -J matrix_large_test
#SBATCH -o large_results_%j.out
#SBATCH -e large_errors_%j.err
#SBATCH -t 00:30:00           # 30 минут
#SBATCH --mem=4G             # 4GB памяти на процесс

echo "=== ТЕСТИРОВАНИЕ БОЛЬШИХ МАТРИЦ ==="
echo "Запущено: $(date)"
echo ""

# Размеры матриц для тестирования (ДОСТАТОЧНО БОЛЬШИЕ!)
SIZES="1024 2048 4096"
# Количество процессов (только квадратные числа для Cannon)
PROCS="1 4 16"

echo "Размеры: $SIZES"
echo "Процессы: $PROCS"
echo ""

# Заголовок таблицы
echo "Алгоритм | Процессы | Размер | Время(с) | MFLOPS" > results_table.txt
echo "---------|----------|--------|----------|--------" >> results_table.txt

for PROC in $PROCS; do
    # Проверяем квадратное ли число процессов (для Cannon)
    SQRT=$(echo "sqrt($PROC)" | bc)
    IS_SQUARE=$(echo "$SQRT * $SQRT" | bc)
    
    for SIZE in $SIZES; do
        echo ""
        echo "--- Тестируем: $PROC процессов, размер $SIZE x $SIZE ---"
        
        # Тестируем Band алгоритм
        echo -n "Band: "
        BAND_TIME=$(mpirun -np $PROC ./band $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')
        BAND_MFLOPS=$(echo "2 * $SIZE * $SIZE * $SIZE / ($BAND_TIME * 1000000)" | bc -l | awk '{printf "%.1f", $1}')
        echo "Время: ${BAND_TIME}s, MFLOPS: ${BAND_MFLOPS}"
        
        # Записываем в таблицу
        echo "Band | $PROC | $SIZE | $BAND_TIME | $BAND_MFLOPS" >> results_table.txt
        
        # Тестируем Cannon только если:
        # 1. Количество процессов - квадрат целого
        # 2. Размер матрицы делится на sqrt(процессов)
        if [ $IS_SQUARE -eq $PROC ] && [ $((SIZE % SQRT)) -eq 0 ]; then
            echo -n "Cannon: "
            CANNON_TIME=$(mpirun -np $PROC ./cannon $SIZE 2>/dev/null | grep "Time:" | awk '{print $2}')
            
            if [ ! -z "$CANNON_TIME" ]; then
                CANNON_MFLOPS=$(echo "2 * $SIZE * $SIZE * $SIZE / ($CANNON_TIME * 1000000)" | bc -l | awk '{printf "%.1f", $1}')
                echo "Время: ${CANNON_TIME}s, MFLOPS: ${CANNON_MFLOPS}"
                echo "Cannon | $PROC | $SIZE | $CANNON_TIME | $CANNON_MFLOPS" >> results_table.txt
                
                # Сравнение
                if [ $(echo "$CANNON_TIME < $BAND_TIME" | bc -l) -eq 1 ]; then
                    SPEEDUP=$(echo "$BAND_TIME / $CANNON_TIME" | bc -l | awk '{printf "%.2f", $1}')
                    echo "  Cannon быстрее Band в $SPEEDUP раз!"
                else
                    SPEEDUP=$(echo "$CANNON_TIME / $BAND_TIME" | bc -l | awk '{printf "%.2f", $1}')
                    echo "  Band быстрее Cannon в $SPEEDUP раз"
                fi
            else
                echo "Ошибка запуска Cannon"
                echo "Cannon | $PROC | $SIZE | ERROR | ERROR" >> results_table.txt
            fi
        else
            echo "Cannon: Пропуск (не подходит размер или кол-во процессов)"
            echo "Cannon | $PROC | $SIZE | SKIP | SKIP" >> results_table.txt
        fi
    done
done

echo ""
echo "=== РЕЗУЛЬТАТЫ ==="
cat results_table.txt

echo ""
echo "Тестирование завершено: $(date)"
