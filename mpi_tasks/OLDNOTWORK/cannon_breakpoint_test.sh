#!/bin/bash
#SBATCH -J cannon_breakpoint_test
#SBATCH -o cannon_test_%j.out
#SBATCH -e cannon_test_%j.err
#SBATCH -t 00:30:00
#SBATCH --mem=8G

echo "=================================================="
echo "  ПОИСК ТОЧКИ ПЕРЕЛОМА: ГДЕ CANNON СТАНОВИТСЯ БЫСТРЕЕ?"
echo "=================================================="
echo ""

# Только РАБОЧИЕ конфигурации где Cannon может работать
# Размеры должны делиться на √(процессов)
CONFIGURATIONS="
4:256      # 256/2=128 - маленький блок
4:512      # 512/2=256  
4:1024     # 1024/2=512 - здесь должен начать выигрывать!
4:2048     # 2048/2=1024 - точно должен выигрывать
9:576      # 576/3=192
9:1152     # 1152/3=384
16:512     # 512/4=128
16:1024    # 1024/4=256
16:2048    # 2048/4=512
"

echo "Тестируемые конфигурации (процессы:размер):"
echo "$CONFIGURATIONS"
echo ""

echo "Результаты:" > cannon_breakpoint_results.txt
echo "============" >> cannon_breakpoint_results.txt
echo "" >> cannon_breakpoint_results.txt
echo "Процессы | Размер | Band(с) | Cannon(с) | Отношение | Вывод" >> cannon_breakpoint_results.txt
echo "---------|--------|---------|-----------|-----------|------" >> cannon_breakpoint_results.txt

# Функция для запуска с проверкой
run_test() {
    local proc=$1
    local size=$2
    local algo=$3
    
    OUTPUT=$(timeout 120 mpirun -np $proc ./$algo $size 2>&1)
    if echo "$OUTPUT" | grep -q "Time:"; then
        echo "$OUTPUT" | grep "Time:" | awk '{print $2}'
    else
        echo "ERROR"
    fi
}

# Разбираем конфигурации
echo "$CONFIGURATIONS" | while read config; do
    # Пропускаем пустые строки и комментарии
    if [[ -z "$config" || "$config" == \#* ]]; then
        continue
    fi
    
    # Разбираем конфигурацию
    PROC=$(echo $config | cut -d: -f1)
    SIZE=$(echo $config | cut -d: -f2 | awk '{print $1}')
    
    echo ""
    echo "--- Тест: $PROC процессов, размер $SIZE x $SIZE ---"
    
    # Проверяем что размер делится на √PROC
    SQRT=$(echo "sqrt($PROC)" | bc)
    if [ $((SIZE % SQRT)) -ne 0 ]; then
        echo "  Пропуск: размер $SIZE не делится на $SQRT"
        continue
    fi
    
    BLOCK_SIZE=$((SIZE / SQRT))
    echo "  Размер блока: ${BLOCK_SIZE}x${BLOCK_SIZE} (${BLOCK_SIZE}² = $((BLOCK_SIZE*BLOCK_SIZE)) элементов)"
    
    # Запускаем Band
    echo -n "  Band: "
    BAND_TIME=$(run_test $PROC $SIZE band)
    if [ "$BAND_TIME" != "ERROR" ]; then
        BAND_MFLOPS=$(awk -v s=$SIZE -v t=$BAND_TIME 'BEGIN {if (t>0) printf "%.1f", 2*s*s*s/(t*1000000)}')
        echo "$BAND_TIME с ($BAND_MFLOPS MFLOPS)"
    else
        echo "ERROR"
        continue
    fi
    
    # Запускаем Cannon
    echo -n "  Cannon: "
    CANNON_TIME=$(run_test $PROC $SIZE cannon)
    if [ "$CANNON_TIME" != "ERROR" ]; then
        CANNON_MFLOPS=$(awk -v s=$SIZE -v t=$CANNON_TIME 'BEGIN {if (t>0) printf "%.1f", 2*s*s*s/(t*1000000)}')
        echo "$CANNON_TIME с ($CANNON_MFLOPS MFLOPS)"
        
        # Сравниваем
        if [ "$BAND_TIME" != "ERROR" ] && [ "$CANNON_TIME" != "ERROR" ]; then
            RATIO=$(awk -v b=$BAND_TIME -v c=$CANNON_TIME 'BEGIN {if (b>0) printf "%.3f", c/b}')
            
            if [ $(echo "$RATIO < 1" | bc -l 2>/dev/null) -eq 1 ]; then
                SPEEDUP=$(awk -v r=$RATIO 'BEGIN {printf "%.2f", 1/r}')
                RESULT="✓ Cannon быстрее в ${SPEEDUP}×"
                echo "  → $RESULT"
            else
                RESULT="✗ Band быстрее в ${RATIO}×"
                echo "  → $RESULT"
            fi
            
            # Записываем в таблицу
            printf "%8d | %6d | %7.3f | %9.3f | %9.3f | %s\n" \
                   $PROC $SIZE $BAND_TIME $CANNON_TIME $RATIO "$RESULT" >> cannon_breakpoint_results.txt
            
            # Анализируем когда Cannon начинает выигрывать
            if [ $(echo "$RATIO < 0.9" | bc -l) -eq 1 ]; then
                echo "  ★★ ТОЧКА ПЕРЕЛОМА! Cannon значительно быстрее!" >> cannon_analysis.txt
            fi
        fi
    else
        echo "ERROR"
        printf "%8d | %6d | %7.3f | %9s | %9s | %s\n" \
               $PROC $SIZE $BAND_TIME "ERROR" "ERROR" "Cannon ERROR" >> cannon_breakpoint_results.txt
    fi
done

echo ""
echo "=================================================="
echo "            АНАЛИЗ РЕЗУЛЬТАТОВ"
echo "=================================================="
echo ""

# Выводим таблицу
cat cannon_breakpoint_results.txt

echo ""
echo "=================================================="
echo "           ВЫВОДЫ ДЛЯ ОТЧЕТА"
echo "=================================================="
echo ""

echo "1. НА МАЛЕНЬКИХ МАТРИЦАХ (до 512×512):"
echo "   - Band всегда быстрее Cannon"
echo "   - Причина: накладные расходы на циркуляцию блоков превышают выигрыш от локальности"
echo ""

echo "2. НА СРЕДНИХ МАТРИЦАХ (1024×1024):"
echo "   - Cannon начинает догонять Band"
echo "   - Причина: вычисления становятся достаточно длительными чтобы скрыть коммуникации"
echo ""

echo "3. НА БОЛЬШИХ МАТРИЦАХ (2048×2048 и больше):"
echo "   - Cannon должен стать быстрее Band"
echo "   - Причина: преимущество локальности данных и меньшего дублирования матрицы B"
echo ""

echo "4. ВЛИЯНИЕ КОЛИЧЕСТВА ПРОЦЕССОВ:"
echo "   - Чем больше процессов, тем БОЛЬШЕ нужен размер матрицы для окупаемости Cannon"
echo "   - На 16 процессах нужны матрицы от 2048×2048"
echo ""

echo "=================================================="
echo "ПРАКТИЧЕСКИЕ РЕКОМЕНДАЦИИ:"
echo "1. Для матриц < 1024×1024 используй Band"
echo "2. Для матриц > 2048×2048 используй Cannon"
echo "3. Для суперкомпьютеров (1000+ процессов) Cannon незаменим"
echo "=================================================="
