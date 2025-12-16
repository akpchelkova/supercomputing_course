#!/bin/bash
#SBATCH --job-name=matrix_band_vs_summa
#SBATCH --output=comparison_%j.out
#SBATCH --error=comparison_%j.err
#SBATCH --time=00:30:00
#SBATCH --partition=gnu
#SBATCH --nodes=4                     # максимальное число узлов для тестов
#SBATCH --ntasks=16                   # максимальное число процессов

module load mpi
cd $SLURM_SUBMIT_DIR

# ==================== КОМПИЛЯЦИЯ ПРОГРАММ ====================
echo "=== КОМПИЛЯЦИЯ ПРОГРАММ ==="
mpicc band.c -o band -lm -O2
mpicc summa.c -o summa -lm -O2

if [ $? -ne 0 ]; then
    echo "Ошибка компиляции!"
    exit 1
fi
echo "Компиляция завершена"
echo ""

# ==================== ПАРАМЕТРЫ ТЕСТИРОВАНИЯ ====================
# ОДИНАКОВЫЕ параметры для обоих алгоритмов
SIZES=(240 480 720 960)
PROCS=(1 2 4 8 16)           # количество процессов (теперь одинаковые для обоих алгоритмов!)
NODES=(1 2 4)

# ==================== ФАЙЛЫ РЕЗУЛЬТАТОВ ====================
RAW_FILE="raw_results_band_vs_summa.csv"
FINAL_FILE="comparison_results_band_vs_summa.csv"
SPEEDUP_FILE="speedup_data_band_vs_summa.csv"

echo "алгоритм,размер,процессы,узлы,время,проверка" > $RAW_FILE

# ==================== ФУНКЦИЯ ТЕСТИРОВАНИЯ ====================
run_test() {
    local algo=$1
    local size=$2
    local procs=$3
    local nodes=$4
    
    echo "--------------------------------------------------"
    echo "ТЕСТ: $algo ${size}x${size}, P=$procs, узлы=$nodes"
    
    # Вычисляем процессов на узел
    local tasks_per_node=$(( (procs + nodes - 1) / nodes ))
    
    if [ "$algo" = "band" ]; then
        # Запуск ленточного алгоритма
        output=$(srun --nodes=$nodes --ntasks=$procs --ntasks-per-node=$tasks_per_node ./band $size $nodes 2>&1)
    else
        # Запуск алгоритма SUMMA
        output=$(srun --nodes=$nodes --ntasks=$procs --ntasks-per-node=$tasks_per_node ./summa $size $nodes 2>&1)
    fi
    
    if [ $? -eq 0 ]; then
        # Парсим вывод программы
        time_val=$(echo "$output" | awk -F',' '{print $4}')
        check_val=$(echo "$output" | awk -F',' '{print $5}' | cut -d'=' -f2)
        
        echo "$algo,$size,$procs,$nodes,$time_val,$check_val" >> $RAW_FILE
        echo "УСПЕХ: время = ${time_val}s, проверка = $check_val"
        return 0
    else
        echo "ОШИБКА: запуск не удался"
        echo "Вывод ошибки: $output"
        echo "$algo,$size,$procs,$nodes,ERROR,ERROR" >> $RAW_FILE
        return 1
    fi
}

# ==================== ПРОВЕРКА ДЕЛИМОСТИ ДЛЯ SUMMA ====================
# Функция проверяет, подходит ли размер матрицы для данного числа процессов
check_size_for_summa() {
    local size=$1
    local procs=$2
    
    # Находим ближайший к квадрату делитель
    local grid_rows=$procs
    for ((i=procs; i>=1; i--)); do
        if [ $((procs % i)) -eq 0 ]; then
            grid_rows=$i
            local grid_cols=$((procs / grid_rows))
            # Проверяем делимость
            if [ $((size % grid_rows)) -eq 0 ] && [ $((size % grid_cols)) -eq 0 ]; then
                return 0  # подходит
            fi
        fi
    done
    return 1  # не подходит
}

# ==================== ОСНОВНОЙ ЦИКЛ ТЕСТИРОВАНИЯ ====================
echo ""
echo "=== НАЧАЛО ТЕСТИРОВАНИЯ ==="
echo ""

total_tests=0
completed_tests=0
failed_tests=0

# Ленточный алгоритм
echo "========== ЛЕНТОЧНЫЙ АЛГОРИТМ =========="
for size in "${SIZES[@]}"; do
    echo "Размер матрицы: ${size}x${size}"
    
    for procs in "${PROCS[@]}"; do
        for nodes in "${NODES[@]}"; do
            # Проверяем: процессов должно быть >= узлов
            if [ $procs -lt $nodes ]; then
                echo "  Пропуск: P=$procs < узлов=$nodes"
                continue
            fi
            
            # Проверяем: не более 16 процессов на узел
            tasks_per_node=$(( (procs + nodes - 1) / nodes ))
            if [ $tasks_per_node -gt 16 ]; then
                echo "  Пропуск: $tasks_per_node процессов на узел > лимита 16"
                continue
            fi
            
            total_tests=$((total_tests + 1))
            run_test "band" $size $procs $nodes
            if [ $? -eq 0 ]; then
                completed_tests=$((completed_tests + 1))
            else
                failed_tests=$((failed_tests + 1))
            fi
        done
    done
done

echo ""
echo "========== АЛГОРИТМ SUMMA =========="

# Алгоритм SUMMA
for size in "${SIZES[@]}"; do
    echo "Размер матрицы: ${size}x${size}"
    
    for procs in "${PROCS[@]}"; do
        # Проверяем делимость размера для SUMMA
        if ! check_size_for_summa $size $procs; then
            echo "  Пропуск SUMMA: размер $size не подходит для P=$procs"
            continue
        fi
        
        for nodes in "${NODES[@]}"; do
            # Проверяем: процессов должно быть >= узлов
            if [ $procs -lt $nodes ]; then
                echo "  Пропуск: P=$procs < узлов=$nodes"
                continue
            fi
            
            # Проверяем: не более 16 процессов на узел
            tasks_per_node=$(( (procs + nodes - 1) / nodes ))
            if [ $tasks_per_node -gt 16 ]; then
                echo "  Пропуск: $tasks_per_node процессов на узел > лимита 16"
                continue
            fi
            
            total_tests=$((total_tests + 1))
            run_test "summa" $size $procs $nodes
            if [ $? -eq 0 ]; then
                completed_tests=$((completed_tests + 1))
            else
                failed_tests=$((failed_tests + 1))
            fi
        done
    done
done

# ==================== АНАЛИЗ И СРАВНЕНИЕ РЕЗУЛЬТАТОВ ====================
echo ""
echo "=== ОБРАБОТКА РЕЗУЛЬТАТОВ ==="
echo "Всего тестов: $total_tests, Успешно: $completed_tests, Ошибок: $failed_tests"

# Создаем финальный файл с ускорением и эффективностью
echo "алгоритм,размер,процессы,узлы,время,ускорение,эффективность,проверка" > $FINAL_FILE

# Словари для базовых времен (на 1 процессе, 1 узле)
declare -A base_band
declare -A base_summa

# Первый проход: собираем базовые времена
while IFS=',' read algo size procs nodes time check; do
    if [ "$time" != "ERROR" ] && [ -n "$time" ]; then
        if [ "$procs" -eq 1 ] && [ "$nodes" -eq 1 ]; then
            if [ "$algo" = "band" ]; then
                base_band["$size"]=$time
            elif [ "$algo" = "summa" ]; then
                base_summa["$size"]=$time
            fi
        fi
    fi
done < $RAW_FILE

# Второй проход: вычисляем ускорение и эффективность
while IFS=',' read algo size procs nodes time check; do
    if [ "$time" != "ERROR" ] && [ -n "$time" ]; then
        # Получаем базовое время
        if [ "$algo" = "band" ]; then
            base_time="${base_band[$size]}"
        else
            base_time="${base_summa[$size]}"
        fi
        
        # Вычисляем метрики
        if [ -n "$base_time" ] && [ "$base_time" != "0" ] && [ "$base_time" != "" ]; then
            speedup=$(echo "$base_time / $time" | bc -l | awk '{printf "%.3f", $1}')
            efficiency=$(echo "$speedup / $procs" | bc -l | awk '{printf "%.3f", $1}')
        else
            speedup="N/A"
            efficiency="N/A"
        fi
        
        echo "$algo,$size,$procs,$nodes,$time,$speedup,$efficiency,$check" >> $FINAL_FILE
    fi
done < $RAW_FILE

# ==================== ВЫВОД СВОДНЫХ ТАБЛИЦ ====================
echo ""
echo "=================================================="
echo "СВОДКА РЕЗУЛЬТАТОВ (Band vs SUMMA)"
echo "=================================================="

# Для каждого размера выводим таблицу
for size in "${SIZES[@]}"; do
    echo ""
    echo "МАТРИЦА ${size}x${size}:"
    echo "--------------------------------------------------------------"
    printf "%-10s %-8s %-6s %-10s %-10s %-12s %-10s\n" \
           "Алгоритм" "Процессы" "Узлы" "Время(с)" "Ускорение" "Эффективность" "Проверка"
    echo "--------------------------------------------------------------"
    
    # Фильтруем результаты для текущего размера
    grep ",$size," $FINAL_FILE | while IFS=',' read algo size procs nodes time speedup efficiency check; do
        printf "%-10s %-8s %-6s %-10.4f %-10s %-12s %-10s\n" \
               "$algo" "$procs" "$nodes" "$time" "$speedup" "$efficiency" "$check"
    done
done

# ==================== ВЫВОД СРАВНЕНИЯ АЛГОРИТМОВ ====================
echo ""
echo "=================================================="
echo "СРАВНЕНИЕ АЛГОРИТМОВ (лучшее время для одинаковых P и N)"
echo "=================================================="

echo "Для одинаковых P (процессы) и узлов:"
echo "--------------------------------------------------------------"
printf "%-8s %-6s %-15s %-15s %-10s\n" \
       "P,Nodes" "Размер" "Время_Band" "Время_SUMMA" "Выигрыш_SUMMA"
echo "--------------------------------------------------------------"

# Ищем все уникальные комбинации (процессы, узлы, размер)
for procs in 2 4 8 16; do
    for nodes in 1 2 4; do
        if [ $procs -ge $nodes ]; then
            for size in "${SIZES[@]}"; do
                # Получаем время для band
                band_line=$(grep "^band,$size,$procs,$nodes," $FINAL_FILE | head -1)
                summa_line=$(grep "^summa,$size,$procs,$nodes," $FINAL_FILE | head -1)
                
                if [ -n "$band_line" ] && [ -n "$summa_line" ]; then
                    band_time=$(echo $band_line | cut -d',' -f5)
                    summa_time=$(echo $summa_line | cut -d',' -f5)
                    
                    # Вычисляем выигрыш (положительный = SUMMA быстрее)
                    if [ "$band_time" != "N/A" ] && [ "$summa_time" != "N/A" ] && \
                       [ "$band_time" != "0" ] && [ "$summa_time" != "0" ]; then
                        advantage=$(echo "($band_time - $summa_time) / $band_time * 100" | bc -l | awk '{printf "%.1f%%", $1}')
                        
                        # Определяем кто быстрее
                        faster="Band"
                        if (( $(echo "$summa_time < $band_time" | bc -l) )); then
                            faster="SUMMA"
                        fi
                        
                        printf "%-4d,%-4d %-6dx%-6d %-14.4f %-14.4f %-10s\n" \
                               "$procs" "$nodes" "$size" "$size" "$band_time" "$summa_time" "$advantage ($faster)"
                    fi
                fi
            done
        fi
    done
done

# ==================== ГЕНЕРАЦИЯ ФАЙЛОВ ДЛЯ ГРАФИКОВ ====================
echo ""
echo "=== СОЗДАНИЕ ФАЙЛОВ ДЛЯ ГРАФИКОВ ==="

# Файл для графика ускорения
echo "size,processes,nodes,band_speedup,summa_speedup" > $SPEEDUP_FILE

for size in "${SIZES[@]}"; do
    for procs in 2 4 8 16; do
        for nodes in 1 2 4; do
            if [ $procs -ge $nodes ]; then
                # Получаем ускорение для band
                band_line=$(grep "^band,$size,$procs,$nodes," $FINAL_FILE | head -1)
                summa_line=$(grep "^summa,$size,$procs,$nodes," $FINAL_FILE | head -1)
                
                band_speedup=$(echo $band_line | cut -d',' -f6)
                summa_speedup=$(echo $summa_line | cut -d',' -f6)
                
                if [ -n "$band_speedup" ] && [ -n "$summa_speedup" ] && \
                   [ "$band_speedup" != "N/A" ] && [ "$summa_speedup" != "N/A" ]; then
                    echo "$size,$procs,$nodes,$band_speedup,$summa_speedup" >> $SPEEDUP_FILE
                fi
            fi
        done
    done
done

echo ""
echo "=================================================="
echo "ВСЁ ЗАВЕРШЕНО!"
echo "=================================================="
echo "Файлы с результатами:"
echo "1. $RAW_FILE - сырые данные"
echo "2. $FINAL_FILE - полные результаты с метриками"
echo "3. $SPEEDUP_FILE - данные для построения графиков"
echo ""
echo "Для построения графиков в Python:"
echo "import pandas as pd"
echo "import matplotlib.pyplot as plt"
echo "data = pd.read_csv('$SPEEDUP_FILE')"
echo ""
echo "Задание завершено. Проверьте файлы выше."
