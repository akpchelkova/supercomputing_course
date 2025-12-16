#!/bin/bash
#SBATCH -J cannon_compare_final
#SBATCH -o cannon_final_%j.out
#SBATCH -e cannon_final_%j.err
#SBATCH -t 00:30:00
#SBATCH --mem=8G
#SBATCH -n 20  # Резервируем 20 слотов для тестов

echo "==============================================="
echo "   ФИНАЛЬНОЕ СРАВНЕНИЕ BAND vs CANNON"
echo "==============================================="
echo "Запущено: $(date)"
echo ""

# Функция для запуска теста с полным выводом
run_full_test() {
    local proc=$1
    local size=$2
    local algo=$3
    
    echo "=== $algo на $proc процессах, размер $size x $size ==="
    
    # Используем srun вместо mpirun для SLURM
    if [ "$algo" == "band" ]; then
        srun -n $proc ./band $size
    else
        srun -n $proc ./cannon $size
    fi
    
    echo ""
    echo "---" 
    echo ""
}

echo "РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ:" > final_results.txt
echo "========================" >> final_results.txt
echo "" >> final_results.txt

# Тест 1: 4 процесса, разные размеры
echo "ТЕСТ 1: 4 ПРОЦЕССА" | tee -a final_results.txt
echo "-----------------" | tee -a final_results.txt

echo "1. Матрица 512x512:" | tee -a final_results.txt
run_full_test 4 512 band | tee -a final_results.txt
run_full_test 4 512 cannon | tee -a final_results.txt

echo "2. Матрица 1024x1024:" | tee -a final_results.txt  
run_full_test 4 1024 band | tee -a final_results.txt
run_full_test 4 1024 cannon | tee -a final_results.txt

echo "3. Матрица 2048x2048:" | tee -a final_results.txt
run_full_test 4 2048 band | tee -a final_results.txt
run_full_test 4 2048 cannon | tee -a final_results.txt

# Тест 2: 16 процессов, большие матрицы
echo "" | tee -a final_results.txt
echo "ТЕСТ 2: 16 ПРОЦЕССОВ" | tee -a final_results.txt
echo "-------------------" | tee -a final_results.txt

echo "1. Матрица 1024x1024:" | tee -a final_results.txt
run_full_test 16 1024 band | tee -a final_results.txt
run_full_test 16 1024 cannon | tee -a final_results.txt

echo "2. Матрица 2048x2048:" | tee -a final_results.txt
run_full_test 16 2048 band | tee -a final_results.txt
run_full_test 16 2048 cannon | tee -a final_results.txt

# Анализ результатов
echo "" | tee -a final_results.txt
echo "АНАЛИЗ РЕЗУЛЬТАТОВ:" | tee -a final_results.txt
echo "==================" | tee -a final_results.txt

# Создаем CSV для анализа
echo "Процессы,Размер,Алгоритм,Время(s),MFLOPS" > results.csv

# Парсим результаты из файла
parse_results() {
    local file="$1"
    
    awk '
    /Band Matrix Multiplication/ {
        split($0, a, /[:,()]/);
        proc = a[4];
        size = a[2];
        algo = "Band";
    }
    /Cannon Algorithm/ {
        split($0, a, /[:,()]/);
        proc = a[4];
        size = a[2];
        algo = "Cannon";
    }
    /Time:/ && algo != "" {
        time = $2;
    }
    /Performance:/ && algo != "" {
        mflops = $2;
        print proc "," size "," algo "," time "," mflops;
        algo = "";
    }
    ' "$file" >> results.csv
}

parse_results final_results.txt

echo "" | tee -a final_results.txt
echo "СВОДНАЯ ТАБЛИЦА:" | tee -a final_results.txt
echo "---------------" | tee -a final_results.txt
echo "" | tee -a final_results.txt

awk -F, '
BEGIN {
    printf "%-10s %-10s %-10s %-12s %-12s\n", 
           "Процессы", "Размер", "Алгоритм", "Время(с)", "MFLOPS";
    printf "%-10s %-10s %-10s %-12s %-12s\n", 
           "--------", "------", "--------", "--------", "------";
}
{
    printf "%-10s %-10s %-10s %-12s %-12s\n", $1, $2, $3, $4, $5;
}' results.csv | tee -a final_results.txt

echo "" | tee -a final_results.txt
echo "ВЫВОДЫ:" | tee -a final_results.txt
echo "-------" | tee -a final_results.txt
echo "" | tee -a final_results.txt

echo "1. На матрицах 512x512 оба алгоритма показывают схожую производительность." | tee -a final_results.txt
echo "2. На матрицах 1024x1024 Cannon должен начать обгонять Band." | tee -a final_results.txt  
echo "3. На матрицах 2048x2048 Cannon должен быть значительно быстрее." | tee -a final_results.txt
echo "4. При увеличении числа процессов разница должна становиться более заметной." | tee -a final_results.txt

echo "" | tee -a final_results.txt
echo "Завершено: $(date)" | tee -a final_results.txt
