#!/bin/bash
echo "=== MPI Task 7: Non-blocking Communication Experiments ==="

# компилируем программу
echo "Compiling balance_nonblocking.c..."
mpicc -o balance_nonblocking balance_nonblocking.c -lm

# очищаем предыдущие результаты и записываем заголовок
echo "processes,total_compute,bytes_per_second,compute_time,comm_time,total_time,overlap_efficiency" > nonblocking_results.csv

# функция для запуска одного эксперимента
run_experiment() {
    local processes=$1
    local compute=$2
    local bytes=$3
    local iterations=$4
    local comment=$5
    
    echo "Running: $comment"
    echo "  Processes: $processes, Compute: ${compute}s, Bytes/sec: $bytes, Iterations: $iterations"
    
    # отправляем задание в slurm и ждем его завершения
    sbatch --wait -n $processes --wrap="mpirun -np $processes ./balance_nonblocking $compute $bytes $iterations"
    sleep 2 # небольшая пауза между заданиями
}

echo "1. Testing different computation/communication ratios (4 processes, 1M bytes/sec)"
run_experiment 4 0.5 1000000 10 "Low compute"
run_experiment 4 1.0 1000000 10 "Medium compute"
run_experiment 4 2.0 1000000 10 "Balanced"
run_experiment 4 4.0 1000000 10 "High compute"
run_experiment 4 8.0 1000000 10 "Very high compute"

echo "2. Testing different communication intensities (4 processes, 2s compute)"
run_experiment 4 2.0 100000 10 "Low comm"
run_experiment 4 2.0 1000000 10 "Medium comm" 
run_experiment 4 2.0 10000000 10 "High comm"
run_experiment 4 2.0 100000000 10 "Very high comm"
run_experiment 4 2.0 1000000000 10 "Extreme comm"

echo "3. Testing different process counts (2s compute, 1M bytes/sec)"
run_experiment 2 2.0 1000000 10 "2 processes"
run_experiment 4 2.0 1000000 10 "4 processes"
run_experiment 8 2.0 1000000 10 "8 processes"
run_experiment 16 2.0 1000000 10 "16 processes"

echo "4. Finding optimal overlap point (4 processes)"
run_experiment 4 0.1 1000000 10 "Test 0.1s"
run_experiment 4 0.2 1000000 10 "Test 0.2s"
run_experiment 4 0.5 1000000 10 "Test 0.5s"
run_experiment 4 1.0 1000000 10 "Test 1.0s"
run_experiment 4 1.5 1000000 10 "Test 1.5s"
run_experiment 4 2.0 1000000 10 "Test 2.0s"
run_experiment 4 3.0 1000000 10 "Test 3.0s"

echo "All experiments completed!"
echo "Results saved in nonblocking_results.csv"
echo "Summary of results:"
cat nonblocking_results.csv
