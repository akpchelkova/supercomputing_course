#!/bin/bash
#SBATCH --job-name=mpi_minmax
#SBATCH --output=mpi_minmax_%j.out
#SBATCH --error=mpi_minmax_%j.err
#SBATCH --time=00:10:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=16
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=2G

# Загружаем необходимые модули (зависит от кластера)
module purge
module load gcc/9.3.0 openmpi/4.0.3

# Переменные
EXECUTABLE="minmax_mpi"
SOURCE_FILE="minmax_mpi.c"
RESULTS_DIR="results_${SLURM_JOB_ID}"
LOG_FILE="performance.log"

# Создаем директорию для результатов
mkdir -p ${RESULTS_DIR}

echo "========================================="
echo "Job ID: ${SLURM_JOB_ID}"
echo "Start time: $(date)"
echo "Number of nodes: ${SLURM_JOB_NUM_NODES}"
echo "Total tasks: ${SLURM_NTASKS}"
echo "========================================="

# Компилируем программу
echo "Compiling MPI program..."
mpicc -O3 -o ${EXECUTABLE} ${SOURCE_FILE} -lm
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi
echo "Compilation successful."

# Запускаем программу с разным количеством процессов
echo -e "\nRunning experiments..."
echo "========================================="

for PROC_COUNT in 1 2 4 8 16 32
do
    if [ ${PROC_COUNT} -le ${SLURM_NTASKS} ]; then
        echo -e "\n--- Running with ${PROC_COUNT} processes ---"
        
        # Запуск MPI программы
        mpirun -np ${PROC_COUNT} ./${EXECUTABLE} > ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt 2>&1
        
        # Извлекаем результаты времени выполнения
        grep "PARALLEL:" ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt | \
            awk -v procs=${PROC_COUNT} '{printf "Procs: %2d, Size: %12d, Time: %9.6f\n", procs, $4, $6}' \
            >> ${RESULTS_DIR}/results_summary.txt
            
        grep "SEQUENTIAL:" ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt | \
            awk '{printf "Procs:  1, Size: %12d, Time: %9.6f (Sequential)\n", $4, $6}' \
            >> ${RESULTS_DIR}/results_summary.txt
    fi
done

# Создаем сводный отчет
echo -e "\nGenerating performance report..."
echo "=========================================" > ${RESULTS_DIR}/performance_report.txt
echo "MPI Min/Max Search Performance Report" >> ${RESULTS_DIR}/performance_report.txt
echo "Job ID: ${SLURM_JOB_ID}" >> ${RESULTS_DIR}/performance_report.txt
echo "Date: $(date)" >> ${RESULTS_DIR}/performance_report.txt
echo "Cluster: $(hostname)" >> ${RESULTS_DIR}/performance_report.txt
echo "=========================================" >> ${RESULTS_DIR}/performance_report.txt
echo -e "\nExecution Times:" >> ${RESULTS_DIR}/performance_report.txt
echo "---------------------------------------------------------" >> ${RESULTS_DIR}/performance_report.txt
echo "Processes | Vector Size      | Time (sec)  | Speedup" >> ${RESULTS_DIR}/performance_report.txt
echo "---------------------------------------------------------" >> ${RESULTS_DIR}/performance_report.txt

# Обрабатываем результаты для создания таблицы
for size in 100 1000 10000 100000 1000000 5000000 10000000 25000000 50000000 100000000
do
    # Находим время для 1 процесса (базовое)
    base_time=$(grep "Size: *${size}" ${RESULTS_DIR}/results_summary.txt | grep "Procs:  1" | head -1 | awk '{print $NF}')
    
    if [ ! -z "$base_time" ]; then
        echo "${size} | 1 proc: ${base_time}" >> ${RESULTS_DIR}/performance_report.txt
        
        for procs in 2 4 8 16 32
        do
            proc_time=$(grep "Size: *${size}" ${RESULTS_DIR}/results_summary.txt | grep "Procs: ${procs}" | head -1 | awk '{print $NF}')
            
            if [ ! -z "$proc_time" ]; then
                if [ "$base_time" != "0.000000" ] && [ "$proc_time" != "0.000000" ]; then
                    speedup=$(echo "scale=2; $base_time / $proc_time" | bc)
                    efficiency=$(echo "scale=2; $speedup / $procs * 100" | bc)
                    echo "${size} | ${procs} procs: ${proc_time} (Speedup: ${speedup}x, Efficiency: ${efficiency}%)" >> ${RESULTS_DIR}/performance_report.txt
                fi
            fi
        done
        echo "" >> ${RESULTS_DIR}/performance_report.txt
    fi
done

echo "=========================================" >> ${RESULTS_DIR}/performance_report.txt

# Выводим сводку
echo -e "\n\n========================================="
echo "Job completed successfully!"
echo "Results saved in directory: ${RESULTS_DIR}"
echo "Performance report: ${RESULTS_DIR}/performance_report.txt"
echo "Raw outputs: ${RESULTS_DIR}/output_*procs.txt"
echo "========================================="

# Копируем основные результаты в основной лог
cat ${RESULTS_DIR}/performance_report.txt > ${LOG_FILE}

echo -e "\nFinal timing summary:"
cat ${RESULTS_DIR}/results_summary.txt

exit 0
