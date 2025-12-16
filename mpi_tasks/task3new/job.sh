#!/bin/bash
#SBATCH --job-name=mpi_comm_tests
#SBATCH --output=mpi_comm_%j.out
#SBATCH --error=mpi_comm_%j.err
#SBATCH --time=00:15:00
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=2G

echo "========================================="
echo "Job ID: ${SLURM_JOB_ID}"
echo "Start time: $(date)"
echo "Cluster: $(hostname)"
echo "========================================="

# Загружаем модули
module purge
module load gcc/9
module load openmpi

echo -e "\n=== System Information ==="
echo "Loaded modules:"
module list
echo -e "\nCompiler info:"
which mpicc
mpicc --version
echo -e "\nMPI implementation:"
mpirun --version

# Переменные
EXECUTABLE="mpi_comm_test"
SOURCE_FILE="mpi_comm_test.c"
RESULTS_DIR="comm_results_${SLURM_JOB_ID}"

# Создаем директорию для результатов
mkdir -p ${RESULTS_DIR}

# Компиляция
echo -e "\n=== Compilation ==="
mpicc -O3 -march=native -o ${EXECUTABLE} ${SOURCE_FILE}
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi
echo "Compilation successful."
ls -lh ./${EXECUTABLE}

echo -e "\n=== Running Communication Tests ==="
echo "========================================="

# Функция для запуска теста с заданным количеством процессов
run_test() {
    local num_procs=$1
    local test_name=$2
    local output_file="${RESULTS_DIR}/${test_name}_${num_procs}procs.txt"
    
    echo -e "\n--- ${test_name} with ${num_procs} processes ---"
    
    # Запускаем MPI программу
    srun -n ${num_procs} ./${EXECUTABLE} > ${output_file} 2>&1
    
    # Проверяем успешность
    if [ $? -eq 0 ]; then
        echo "✓ Test completed successfully"
        
        # Извлекаем основные результаты
        echo "Key results:"
        grep -E "(PING-PONG|ALL-TO-ALL|SCALING|latency:)" ${output_file} | head -5
        
        # Сохраняем время выполнения
        grep "AvgTime" ${output_file} | tail -3 >> ${RESULTS_DIR}/summary.txt
    else
        echo "✗ Test failed"
    fi
}

# Функция для тестирования разных топологий
run_topology_test() {
    local num_procs=$1
    local nodes=$2
    local ppn=$3
    local test_name=$4
    
    echo -e "\n--- ${test_name} (${nodes} nodes × ${ppn} procs) ---"
    
    if [ ${nodes} -eq 1 ]; then
        # Все процессы на одном узле
        srun -n ${num_procs} --ntasks-per-node=${num_procs} ./${EXECUTABLE} > \
            ${RESULTS_DIR}/topology_${test_name}.txt 2>&1
    else
        # Процессы распределены по узлам
        srun -n ${num_procs} --ntasks-per-node=${ppn} ./${EXECUTABLE} > \
            ${RESULTS_DIR}/topology_${test_name}.txt 2>&1
    fi
    
    echo "Results saved to topology_${test_name}.txt"
}

# Основные тесты с разным количеством процессов
echo -e "\n=== Testing Different Process Counts ==="

# Тестируем разное количество процессов
PROCESS_COUNTS="2 4 8 16 32"
for procs in ${PROCESS_COUNTS}; do
    if [ ${procs} -le ${SLURM_NTASKS} ]; then
        run_test ${procs} "full_test"
    fi
done

# Специальные тесты для 2 процессов (ping-pong)
echo -e "\n=== Special Ping-Pong Tests ==="
run_test 2 "pingpong_detailed"

# Тестирование разных топологий
echo -e "\n=== Testing Different Topologies ==="

# 1. Все процессы на одном узле
run_topology_test 8 1 8 "single_node"

# 2. Процессы распределены по 2 узлам
if [ ${SLURM_JOB_NUM_NODES} -ge 2 ]; then
    run_topology_test 8 2 4 "two_nodes"
fi

# 3. Процессы распределены по 4 узлам
if [ ${SLURM_JOB_NUM_NODES} -ge 4 ]; then
    run_topology_test 8 4 2 "four_nodes"
fi

# Тестирование с разными размерами сообщений (специальный режим)
echo -e "\n=== Message Size Scaling Test ==="

# Создаем конфигурационный файл для тестирования
cat > ${RESULTS_DIR}/test_config.txt << EOF
# MPI Communication Test Configuration
Job ID: ${SLURM_JOB_ID}
Date: $(date)
Total Nodes: ${SLURM_JOB_NUM_NODES}
Total Tasks: ${SLURM_NTASKS}
CPUs per Task: ${SLURM_CPUS_PER_TASK}
Memory per CPU: ${SLURM_MEM_PER_CPU}

Tested Process Counts: ${PROCESS_COUNTS}
EOF

# Создаем сводный отчет
echo -e "\n=== Generating Summary Report ==="
echo "=========================================" > ${RESULTS_DIR}/final_report.txt
echo "MPI COMMUNICATION PERFORMANCE REPORT" >> ${RESULTS_DIR}/final_report.txt
echo "Job ID: ${SLURM_JOB_ID}" >> ${RESULTS_DIR}/final_report.txt
echo "Date: $(date)" >> ${RESULTS_DIR}/final_report.txt
echo "=========================================" >> ${RESULTS_DIR}/final_report.txt

# Собираем результаты из всех файлов
for procs in ${PROCESS_COUNTS}; do
    if [ ${procs} -le ${SLURM_NTASKS} ]; then
        file="${RESULTS_DIR}/full_test_${procs}procs.txt"
        if [ -f "${file}" ]; then
            echo -e "\n--- ${procs} Processes ---" >> ${RESULTS_DIR}/final_report.txt
            grep -E "(PING-PONG|ALL-TO-ALL|SCALING)" ${file} | head -10 >> ${RESULTS_DIR}/final_report.txt
        fi
    fi
done

# Добавляем латентность
latency_file="${RESULTS_DIR}/pingpong_detailed_2procs.txt"
if [ -f "${latency_file}" ]; then
    echo -e "\n=== Latency Measurements ===" >> ${RESULTS_DIR}/final_report.txt
    grep -i "latency" ${latency_file} >> ${RESULTS_DIR}/final_report.txt
fi

echo -e "\n=== Performance Summary ===" >> ${RESULTS_DIR}/final_report.txt
echo "MsgSize | 2procs Time | 4procs Time | 8procs Time | 16procs Time | Bandwidth Trend" >> ${RESULTS_DIR}/final_report.txt
echo "-------------------------------------------------------------------------------" >> ${RESULTS_DIR}/final_report.txt

# Анализируем производительность для 1MB сообщений
for procs in 2 4 8 16; do
    if [ ${procs} -le ${SLURM_NTASKS} ]; then
        file="${RESULTS_DIR}/full_test_${procs}procs.txt"
        if [ -f "${file}" ]; then
            grep "1048576" ${file} | head -1 >> ${RESULTS_DIR}/final_report.txt
        fi
    fi
done

# Завершение
echo -e "\n=== Job Completion ==="
echo "All tests completed!"
echo "Results directory: ${RESULTS_DIR}"
echo -e "\nMain report file: ${RESULTS_DIR}/final_report.txt"
echo -e "\nQuick overview of results:"
tail -30 ${RESULTS_DIR}/final_report.txt

echo -e "\n========================================="
echo "To view full results:"
echo "cat ${RESULTS_DIR}/final_report.txt"
echo "========================================="

# Копируем основные результаты в общий файл
cp ${RESULTS_DIR}/final_report.txt mpi_communication_report.txt

exit 0
