#!/bin/bash
#SBATCH -J ultimate_matrix_test
#SBATCH -o ultimate_%j.out
#SBATCH -e ultimate_%j.err
#SBATCH -t 01:00:00
#SBATCH --mem=16G

echo "================================================"
echo "     ULTIMATE TEST: BAND vs CANNON"
echo "================================================"
echo "Запущено: $(date)"
echo ""

# Функция для безопасного извлечения времени
get_time() {
    echo "$1" | grep "Time:" | awk '{print $2}' | head -1
}

# Функция для вычисления MFLOPS
calculate_mflops() {
    local size="$1"
    local time="$2"
    awk -v s="$size" -v t="$time" 'BEGIN {
        if (t > 0 && t != "TIMEOUT" && t != "ERROR") {
            mflops = 2.0 * s * s * s / (t * 1000000);
            printf "%.1f", mflops;
        } else {
            printf "N/A";
        }
    }'
}

# Функция для запуска алгоритма с таймаутом
run_with_timeout() {
    local proc="$1"
    local size="$2"
    local algo="$3"
    local timeout_val=60  # 60 секунд максимум
    
    if [ "$algo" == "band" ]; then
        timeout $timeout_val mpirun -np $proc ./band $size 2>&1
    else
        # Для Cannon проверяем можно ли запустить
        local sqrt_proc=$(echo "sqrt($proc)" | bc 2>/dev/null)
        if [ $? -eq 0 ] && [ $((size % sqrt_proc)) -eq 0 ] && [ $((sqrt_proc * sqrt_proc)) -eq $proc ]; then
            timeout $timeout_val mpirun -np $proc ./cannon $size 2>&1
        else
            echo "Cannon: SKIP (неподходящая конфигурация)"
        fi
    fi
}

# Матрицы от маленьких до ОЧЕНЬ больших
SIZES="64 128 256 512 1024 2048 4096"
# Процессы для тестирования
PROCS="1 4 9 16"

echo "Тестируемые размеры: $SIZES"
echo "Тестируемые процессы: $PROCS"
echo ""

# Создаем CSV файл для результатов
echo "Algorithm,Processes,Size,Time(s),MFLOPS,Status" > results.csv

# Главный цикл тестирования
for PROC in $PROCS; do
    echo ""
    echo "────────────────────────────────────────────"
    echo "   ТЕСТ НА $PROC ПРОЦЕССАХ"
    echo "────────────────────────────────────────────"
    
    for SIZE in $SIZES; do
        echo ""
        echo "  Размер: ${SIZE}x${SIZE}"
        echo "  ----------------------------------"
        
        # ========== BAND ALGORITHM ==========
        echo -n "    Band: "
        BAND_OUTPUT=$(run_with_timeout $PROC $SIZE band)
        
        if echo "$BAND_OUTPUT" | grep -q "Time:"; then
            BAND_TIME=$(get_time "$BAND_OUTPUT")
            BAND_MFLOPS=$(calculate_mflops $SIZE "$BAND_TIME")
            echo "${BAND_TIME}s, ${BAND_MFLOPS} MFLOPS"
            echo "band,$PROC,$SIZE,$BAND_TIME,$BAND_MFLOPS,OK" >> results.csv
        elif echo "$BAND_OUTPUT" | grep -q "TIMEOUT"; then
            echo "TIMEOUT (>60s)"
            echo "band,$PROC,$SIZE,TIMEOUT,N/A,TIMEOUT" >> results.csv
        else
            echo "ERROR"
            echo "band,$PROC,$SIZE,ERROR,N/A,ERROR" >> results.csv
        fi
        
        # ========== CANNON ALGORITHM ==========
        echo -n "    Cannon: "
        CANNON_OUTPUT=$(run_with_timeout $PROC $SIZE cannon)
        
        if echo "$CANNON_OUTPUT" | grep -q "Time:"; then
            CANNON_TIME=$(get_time "$CANNON_OUTPUT")
            CANNON_MFLOPS=$(calculate_mflops $SIZE "$CANNON_TIME")
            echo "${CANNON_TIME}s, ${CANNON_MFLOPS} MFLOPS"
            echo "cannon,$PROC,$SIZE,$CANNON_TIME,$CANNON_MFLOPS,OK" >> results.csv
            
            # Сравнение если оба отработали
            if [ -n "$BAND_TIME" ] && [ "$BAND_TIME" != "TIMEOUT" ] && [ "$BAND_TIME" != "ERROR" ]; then
                RATIO=$(awk -v b="$BAND_TIME" -v c="$CANNON_TIME" 'BEGIN {
                    if (b > 0 && c > 0) {
                        ratio = c / b;
                        if (ratio < 1) {
                            printf "Cannon быстрее в %.2fx", 1/ratio;
                        } else {
                            printf "Band быстрее в %.2fx", ratio;
                        }
                    }
                }')
                echo "    → $RATIO"
            fi
            
        elif echo "$CANNON_OUTPUT" | grep -q "SKIP"; then
            echo "SKIP (размер $SIZE не делится на √$PROC)"
            echo "cannon,$PROC,$SIZE,SKIP,N/A,SKIP" >> results.csv
        elif echo "$CANNON_OUTPUT" | grep -q "TIMEOUT"; then
            echo "TIMEOUT (>60s)"
            echo "cannon,$PROC,$SIZE,TIMEOUT,N/A,TIMEOUT" >> results.csv
        else
            echo "ERROR"
            echo "cannon,$PROC,$SIZE,ERROR,N/A,ERROR" >> results.csv
        fi
    done
done

echo ""
echo "================================================"
echo "          ОБРАБОТКА РЕЗУЛЬТАТОВ"
echo "================================================"

# Генерируем красивую таблицу
echo ""
echo "СВОДНАЯ ТАБЛИЦА РЕЗУЛЬТАТОВ:"
echo ""

awk -F, '
BEGIN {
    printf "%-10s %-8s %-10s %-12s %-10s %-10s\n", 
           "Algorithm", "Procs", "Size", "Time(s)", "MFLOPS", "Status";
    printf "%-10s %-8s %-10s %-12s %-10s %-10s\n", 
           "---------", "-----", "----", "-------", "------", "------";
}
NR > 1 {
    printf "%-10s %-8s %-10s %-12s %-10s %-10s\n", 
           $1, $2, $3, $4, $5, $6;
}' results.csv

echo ""
echo "АНАЛИЗ МАСШТАБИРУЕМОСТИ:"
echo ""

# Анализ масштабируемости Band
echo "Band алгоритм - ускорение относительно 1 процесса:"
for PROC in 4 9 16; do
    for SIZE in 256 512 1024 2048; do
        TIME1=$(awk -F, -v p=1 -v s=$SIZE -v a="band" '$1==a && $2==p && $3==s {print $4}' results.csv 2>/dev/null)
        TIMEP=$(awk -F, -v p=$PROC -v s=$SIZE -v a="band" '$1==a && $2==p && $3==s {print $4}' results.csv 2>/dev/null)
        
        if [ -n "$TIME1" ] && [ -n "$TIMEP" ] && [ "$TIME1" != "ERROR" ] && [ "$TIMEP" != "ERROR" ] && [ "$TIME1" != "TIMEOUT" ] && [ "$TIMEP" != "TIMEOUT" ]; then
            SPEEDUP=$(awk -v t1="$TIME1" -v tp="$TIMEP" 'BEGIN {if (tp > 0) printf "%.2f", t1/tp}')
            if [ -n "$SPEEDUP" ]; then
                echo "  Size $SIZE: 1 proc → $PROC procs: $TIME1 → $TIMEP (ускорение: ${SPEEDUP}x)"
            fi
        fi
    done
done

echo ""
echo "СРАВНЕНИЕ BAND vs CANNON:"
echo ""

# Находим размер, где Cannon становится быстрее
echo "Поиск точки перелома (где Cannon становится быстрее Band):"
for PROC in 4 9 16; do
    for SIZE in 256 512 1024 2048 4096; do
        BAND_TIME=$(awk -F, -v p=$PROC -v s=$SIZE -v a="band" '$1==a && $2==p && $3==s && $6=="OK" {print $4}' results.csv 2>/dev/null)
        CANNON_TIME=$(awk -F, -v p=$PROC -v s=$SIZE -v a="cannon" '$1==a && $2==p && $3==s && $6=="OK" {print $4}' results.csv 2>/dev/null)
        
        if [ -n "$BAND_TIME" ] && [ -n "$CANNON_TIME" ]; then
            RATIO=$(awk -v b="$BAND_TIME" -v c="$CANNON_TIME" 'BEGIN {if (b > 0) printf "%.3f", c/b}')
            if [ $(echo "$RATIO < 1" | bc -l 2>/dev/null) -eq 1 ]; then
                SPEEDUP=$(awk -v r="$RATIO" 'BEGIN {printf "%.2f", 1/r}')
                echo "  $PROC процессов, размер $SIZE: Cannon быстрее в ${SPEEDUP}x ($BAND_TIME → $CANNON_TIME)"
            fi
        fi
    done
done

echo ""
echo "================================================"
echo "РЕЗУЛЬТАТЫ СОХРАНЕНЫ В:"
echo "  1. results.csv - CSV формат для анализа"
echo "  2. ultimate_${SLURM_JOB_ID}.out - полный вывод"
echo ""
echo "Завершено: $(date)"
echo "================================================"
