#!/bin/bash
#SBATCH --job-name=matrix_benchmark
#SBATCH --output=benchmark_%j.out
#SBATCH --error=benchmark_%j.err
#SBATCH --time=02:00:00          # 2 часа
#SBATCH --nodes=4                # 4 узла
#SBATCH --ntasks-per-node=4      # 4 задачи на узел

module load openmpi

# Компиляция с оптимизациями
echo "Compiling with optimizations..."
mpicc -O3 -march=native -ffast-math -o band band.c -lm
mpicc -O3 -march=native -ffast-math -o cannon cannon.c -lm

# Создаем файл для результатов
RESULT_FILE="results_summary_${SLURM_JOB_ID}.txt"
echo "Matrix Multiplication Benchmark Results" > $RESULT_FILE
echo "Job ID: ${SLURM_JOB_ID}" >> $RESULT_FILE
echo "Date: $(date)" >> $RESULT_FILE
echo "========================================" >> $RESULT_FILE

# Функция для извлечения времени из вывода программы
extract_time() {
    grep "Time:" | awk '{print $2}'
}

extract_flops() {
    grep "Performance:" | awk '{print $2}'
}

# Тест 1: Большие матрицы с разным числом процессов
echo "TEST 1: Large matrices (4096x4096)" | tee -a $RESULT_FILE

# 16 процессов
echo "16 processes:" >> $RESULT_FILE
echo -n "Band: " >> $RESULT_FILE
mpirun -np 16 ./band 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo -n "Cannon: " >> $RESULT_FILE
mpirun -np 16 ./cannon 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo "" >> $RESULT_FILE

# 9 процессов
echo "9 processes:" >> $RESULT_FILE
echo -n "Band: " >> $RESULT_FILE
mpirun -np 9 ./band 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo -n "Cannon: " >> $RESULT_FILE
mpirun -np 9 ./cannon 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo "" >> $RESULT_FILE

# 4 процесса
echo "4 processes:" >> $RESULT_FILE
echo -n "Band: " >> $RESULT_FILE
mpirun -np 4 ./band 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo -n "Cannon: " >> $RESULT_FILE
mpirun -np 4 ./cannon 4096 2>/dev/null | extract_time >> $RESULT_FILE
echo "" >> $RESULT_FILE

# Тест 2: Масштабируемость (фиксированный размер, разное число процессов)
echo "" >> $RESULT_FILE
echo "TEST 2: Scalability (2048x2048)" >> $RESULT_FILE

for np in 1 4 9 16; do
    echo "$np processes:" >> $RESULT_FILE
    
    if [ $np -eq 1 ] || [ $np -eq 4 ] || [ $np -eq 9 ] || [ $np -eq 16 ]; then
        echo -n "Band Time: " >> $RESULT_FILE
        mpirun -np $np ./band 2048 2>/dev/null | extract_time >> $RESULT_FILE
        
        # Для Cannon проверяем, что размер делится на sqrt(np)
        sqrt_np=$(echo "sqrt($np)" | bc)
        if [ $((2048 % sqrt_np)) -eq 0 ]; then
            echo -n "Cannon Time: " >> $RESULT_FILE
            mpirun -np $np ./cannon 2048 2>/dev/null | extract_time >> $RESULT_FILE
        else
            echo "Cannon: N/A (2048 not divisible by $sqrt_np)" >> $RESULT_FILE
        fi
    else
        echo "Band Time: " >> $RESULT_FILE
        mpirun -np $np ./band 2048 2>/dev/null | extract_time >> $RESULT_FILE
        echo "Cannon: N/A (not perfect square)" >> $RESULT_FILE
    fi
    
    echo "" >> $RESULT_FILE
done

# Тест 3: Производительность в MFLOPS для 4096
echo "" >> $RESULT_FILE
echo "TEST 3: Performance in MFLOPS (4096x4096)" >> $RESULT_FILE
echo "16 processes:" >> $RESULT_FILE
echo -n "Band: " >> $RESULT_FILE
mpirun -np 16 ./band 4096 2>/dev/null | extract_flops >> $RESULT_FILE
echo -n "Cannon: " >> $RESULT_FILE
mpirun -np 16 ./cannon 4096 2>/dev/null | extract_flops >> $RESULT_FILE

echo "" >> $RESULT_FILE
echo "Benchmark complete!" >> $RESULT_FILE
echo "Detailed results in: benchmark_${SLURM_JOB_ID}.out" >> $RESULT_FILE

# Выводим краткие результаты
echo ""
echo "=== BRIEF RESULTS ==="
cat $RESULT_FILE
