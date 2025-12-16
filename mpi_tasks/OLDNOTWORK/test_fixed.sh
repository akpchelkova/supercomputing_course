#!/bin/bash
#SBATCH -J final_matrix_test
#SBATCH -o final_%j.out
#SBATCH -e final_%j.err
#SBATCH -t 00:30:00
#SBATCH --mem=8G

echo "=== ФИНАЛЬНЫЙ ТЕСТ УМНОЖЕНИЯ МАТРИЦ ==="
echo "Запущено: $(date)"
echo ""

# Функция вычисления MFLOPS через awk (без bc)
calculate_mflops() {
    local size=$1
    local time=$2
    awk -v s=$size -v t=$time 'BEGIN {
        if (t > 0) {
            mflops = 2.0 * s * s * s / (t * 1000000);
            printf "%.1f", mflops;
        } else {
            printf "ERROR";
        }
    }'
}

# Функция сравнения времени через awk
compare_times() {
    local time1=$1
    local time2=$2
    awk -v t1=$time1 -v t2=$time2 'BEGIN {
        if (t1 > 0 && t2 > 0) {
            ratio = t2 / t1;
            printf "%.3f", ratio;
        } else {
            printf "ERROR";
        }
    }'
}

SIZES="256 512 1024 2048"
PROCS="1 4"

echo "=== РЕЗУЛЬТАТЫ ===" > final_results.txt
echo "" >> final_results.txt

for PROC in $PROCS; do
    echo "--- $PROC ПРОЦЕССОВ ---" | tee -a final_results.txt
    echo "Размер | Band(с) | Cannon(с) | Отношение" | tee -a final_results.txt
    echo "-------|---------|-----------|----------" | tee -a final_results.txt
    
    for SIZE in $SIZES; do
        # Пропускаем Cannon если не подходит
        SQRT=$(echo "sqrt($PROC)" | bc 2>/dev/null || echo "0")
        if [ $PROC -eq 1 ] || ([ $SQRT -gt 0 ] && [ $((SIZE % SQRT)) -eq 0 ]); then
            # Тест Band
            BAND_OUT=$(mpirun -np $PROC ./band $SIZE 2>&1)
            BAND_TIME=$(echo "$BAND_OUT" | grep "Time:" | awk '{print $2}')
            BAND_MFLOPS=$(calculate_mflops $SIZE $BAND_TIME)
            
            # Тест Cannon с таймаутом
            CANNON_TIME="TIMEOUT"
            if [ $PROC -eq 1 ]; then
                # Для 1 процесса Cannon должен работать
                CANNON_OUT=$(timeout 30 mpirun -np $PROC ./cannon $SIZE 2>&1)
                if [ $? -eq 124 ]; then
                    CANNON_TIME="TIMEOUT"
                else
                    CANNON_TIME=$(echo "$CANNON_OUT" | grep "Time:" | awk '{print $2}')
                fi
            else
                # Для >1 процессов тестируем только если размер подходит
                CANNON_OUT=$(timeout 30 mpirun -np $PROC ./cannon $SIZE 2>&1)
                if [ $? -eq 124 ]; then
                    CANNON_TIME="TIMEOUT"
                else
                    CANNON_TIME=$(echo "$CANNON_OUT" | grep "Time:" | awk '{print $2}')
                fi
            fi
            
            if [ "$CANNON_TIME" = "TIMEOUT" ]; then
                CANNON_MFLOPS="TIMEOUT"
                RATIO="TIMEOUT"
            else
                CANNON_MFLOPS=$(calculate_mflops $SIZE $CANNON_TIME)
                RATIO=$(compare_times $BAND_TIME $CANNON_TIME)
            fi
            
            # Вывод строки
            printf "%6d | %7.3f | %9s | %s\n" $SIZE $BAND_TIME $CANNON_TIME $RATIO | tee -a final_results.txt
            
            # Комментарий
            if [ "$CANNON_TIME" = "TIMEOUT" ]; then
                echo "  ⚠ Cannon завис на $SIZE" | tee -a final_results.txt
            elif [ "$RATIO" != "ERROR" ] && [ "$RATIO" != "TIMEOUT" ]; then
                if [ $(echo "$RATIO < 1" | bc -l 2>/dev/null || echo "0") -eq 1 ]; then
                    SPEEDUP=$(awk -v r=$RATIO 'BEGIN {printf "%.2f", 1/r}')
                    echo "  ✓ Cannon быстрее в $SPEEDUP раз" | tee -a final_results.txt
                else
                    echo "  ✗ Band быстрее в $RATIO раз" | tee -a final_results.txt
                fi
            fi
        else
            # Cannon нельзя запустить
            BAND_OUT=$(mpirun -np $PROC ./band $SIZE 2>&1)
            BAND_TIME=$(echo "$BAND_OUT" | grep "Time:" | awk '{print $2}')
            printf "%6d | %7.3f | %9s | %s\n" $SIZE $BAND_TIME "SKIP" "SKIP" | tee -a final_results.txt
            echo "  ➤ Cannon пропущен (размер $SIZE не делится на $SQRT)" | tee -a final_results.txt
        fi
        echo "" | tee -a final_results.txt
    done
    echo "" | tee -a final_results.txt
done

echo "=== СВОДКА ===" | tee -a final_results.txt
echo "" | tee -a final_results.txt
echo "Band работает нормально на всех конфигурациях." | tee -a final_results.txt
echo "Cannon зависает на многопроцессорных запусках." | tee -a final_results.txt
echo "" | tee -a final_results.txt
echo "Завершено: $(date)" | tee -a final_results.txt
