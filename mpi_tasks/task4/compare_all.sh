#!/bin/bash
#SBATCH --job-name=matrix_compare_final
#SBATCH --output=final_results_%j.out
#SBATCH --error=final_results_%j.err
#SBATCH --time=01:00:00
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=4

module load openmpi

echo "=========================================="
echo "FINAL MATRIX MULTIPLICATION COMPARISON"
echo "Start: $(date)"
echo "=========================================="
echo ""

# Компиляция всех программ
echo "Compiling programs..."
mpicc -O3 -march=native -ffast-math -o band_improved band_improved.c -lm
mpicc -O3 -march=native -ffast-math -o cannon_simple cannon_simple.c -lm
echo "Compilation complete."
echo ""

# Функция для запуска теста
run_test() {
    local algo=$1
    local np=$2
    local size=$3
    
    echo "=== $algo: $np processes, N=$size ==="
    
    if [ "$algo" == "Band" ]; then
        srun --mpi=pmix -n $np ./band_improved $size 2>&1 | grep -E "(Time:|Performance:|checksum:|WARNING:|ERROR:)"
    elif [ "$algo" == "Cannon" ]; then
        srun --mpi=pmix -n $np ./cannon_simple $size 2>&1 | grep -E "(Time:|Performance:|checksum:|Grid:|ERROR:)"
    fi
    
    local exit_code=${PIPESTATUS[0]}
    if [ $exit_code -ne 0 ]; then
        echo "FAILED with exit code $exit_code"
    fi
    echo ""
}

# Функция для проверки корректности конфигурации
check_config() {
    local np=$1
    local size=$2
    local algo=$3
    
    if [ "$algo" == "Cannon" ]; then
        # Проверяем квадрат ли число процессов
        sqrt_np=$(echo "sqrt($np)" | bc)
        if [ $((sqrt_np * sqrt_np)) -ne $np ]; then
            return 1
        fi
        
        # Проверяем делимость
        if [ $((size % sqrt_np)) -ne 0 ]; then
            return 1
        fi
    fi
    
    return 0
}

# Основные тесты
echo "TEST 1: Performance scaling with fixed size (2048x2048)"
echo "------------------------------------------------------"
for np in 1 4 9 16; do
    echo ""
    echo "--- $np Processes ---"
    
    # Band алгоритм (работает для любого числа процессов)
    if [ $((2048 % np)) -eq 0 ]; then
        run_test "Band" $np 2048
    else
        echo "Band: N/A (2048 not divisible by $np)"
    fi
    
    # Cannon алгоритм
    if check_config $np 2048 "Cannon"; then
        run_test "Cannon" $np 2048
    else
        echo "Cannon: N/A (configuration not valid)"
    fi
done

echo ""
echo "TEST 2: Large matrix (4096x4096) with optimal process counts"
echo "------------------------------------------------------------"
for np in 4 16; do
    echo ""
    
    # Band
    if [ $((4096 % np)) -eq 0 ]; then
        run_test "Band" $np 4096
    else
        echo "Band $np processes: N/A (4096 not divisible by $np)"
    fi
    
    # Cannon
    if check_config $np 4096 "Cannon"; then
        run_test "Cannon" $np 4096
    else
        echo "Cannon $np processes: N/A (configuration not valid)"
    fi
done

echo ""
echo "TEST 3: Small/Medium sizes for algorithm comparison"
echo "---------------------------------------------------"
for size in 512 1024; do
    echo ""
    echo "--- Size: ${size}x${size} ---"
    
    for np in 1 4; do
        echo ""
        echo "  $np processes:"
        
        # Band
        if [ $((size % np)) -eq 0 ]; then
            echo -n "    Band: "
            srun --mpi=pmix -n $np ./band_improved $size 2>&1 | grep "Time:" | awk '{printf "%.4f sec, ", $2}'
            srun --mpi=pmix -n $np ./band_improved $size 2>&1 | grep "Performance:" | awk '{printf "%.0f MFLOPS\n", $2}'
        fi
        
        # Cannon
        if check_config $np $size "Cannon"; then
            echo -n "    Cannon: "
            srun --mpi=pmix -n $np ./cannon_simple $size 2>&1 | grep "Time:" | awk '{printf "%.4f sec, ", $2}'
            srun --mpi=pmix -n $np ./cannon_simple $size 2>&1 | grep "Performance:" | awk '{printf "%.0f MFLOPS\n", $2}'
        fi
    done
done

echo ""
echo "TEST 4: Speedup calculation"
echo "---------------------------"
echo "Matrix: 2048x2048"
echo "Reference (1 process Band):"
ref_time=$(srun --mpi=pmix -n 1 ./band_improved 2048 2>&1 | grep "Time:" | awk '{print $2}')
echo "  Time: $ref_time seconds"

for np in 4 16; do
    if [ $((2048 % np)) -eq 0 ]; then
        time_np=$(srun --mpi=pmix -n $np ./band_improved 2048 2>&1 | grep "Time:" | awk '{print $2}')
        speedup=$(echo "$ref_time / $time_np" | bc -l)
        echo "  Band $np processes: $time_np sec, Speedup: $(printf "%.2f" $speedup)x"
    fi
done

echo ""
echo "=========================================="
echo "End: $(date)"
echo "Results saved to: final_results_${SLURM_JOB_ID}.out"
echo "=========================================="
