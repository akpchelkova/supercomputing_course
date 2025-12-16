#!/bin/bash
#SBATCH --job-name=dot_product
#SBATCH --output=mpi_dot_%j.out
#SBATCH --error=mpi_dot_%j.err
#SBATCH --time=00:10:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=16
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=2G

echo "========================================="
echo "Job ID: ${SLURM_JOB_ID}"
echo "Start time: $(date)"
echo "Number of nodes: ${SLURM_JOB_NUM_NODES}"
echo "Total tasks: ${SLURM_NTASKS}"
echo "========================================="

# Загружаем модули (адаптируйте под ваш кластер)
module purge
module load gcc/9
module load openmpi

echo -e "\nLoaded modules:"
module list

echo -e "\nCompiler info:"
which mpicc
mpicc --version

# Переменные
EXECUTABLE="dot_product"
SOURCE_FILE="dot_product.c"
RESULTS_DIR="dot_results_${SLURM_JOB_ID}"

# Создаем директорию для результатов
mkdir -p ${RESULTS_DIR}

# Компилируем программу
echo -e "\nCompiling MPI program..."
mpicc -O3 -march=native -ffast-math -o ${EXECUTABLE} ${SOURCE_FILE} -lm
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi
echo "Compilation successful."

# Проверяем исполняемый файл
ls -lh ./${EXECUTABLE}

echo -e "\nRunning experiments..."
echo "========================================="

# Максимальное число процессов
MAX_PROCS=${SLURM_NTASKS}
if [ ${MAX_PROCS} -gt 32 ]; then
    MAX_PROCS=32
fi

# Тестируем разное количество процессов
PROC_LIST="1 2 4 8 16"
if [ ${MAX_PROCS} -ge 32 ]; then
    PROC_LIST="${PROC_LIST} 32"
fi

for PROC_COUNT in ${PROC_LIST}
do
    if [ ${PROC_COUNT} -le ${MAX_PROCS} ]; then
        echo -e "\n--- Running with ${PROC_COUNT} processes ---"
        
        # Запуск MPI программы
        srun -n ${PROC_COUNT} ./${EXECUTABLE} > ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt 2>&1
        
        # Проверяем успешность
        if [ $? -eq 0 ]; then
            echo "Run completed successfully."
        else
            echo "WARNING: Run with ${PROC_COUNT} processes failed."
        fi
        
        # Извлекаем результаты
        echo "Timing results:"
        grep -E "(PARALLEL:|SEQUENTIAL:)" ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt | tail -10
    fi
done

# Создаем сводный отчет
echo -e "\nGenerating summary report..."
echo "=========================================" > ${RESULTS_DIR}/summary_report.txt
echo "MPI Dot Product Performance Summary" >> ${RESULTS_DIR}/summary_report.txt
echo "Job ID: ${SLURM_JOB_ID}" >> ${RESULTS_DIR}/summary_report.txt
echo "Date: $(date)" >> ${RESULTS_DIR}/summary_report.txt
echo "=========================================" >> ${RESULTS_DIR}/summary_report.txt

for PROC_COUNT in ${PROC_LIST}
do
    if [ ${PROC_COUNT} -le ${MAX_PROCS} ]; then
        echo -e "\n--- ${PROC_COUNT} processes ---" >> ${RESULTS_DIR}/summary_report.txt
        grep -E "(PARALLEL:|SEQUENTIAL:)" ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt >> ${RESULTS_DIR}/summary_report.txt
    fi
done

# Анализ ускорения
echo -e "\nCalculating speedup..."
echo -e "\nSpeedup Analysis:" >> ${RESULTS_DIR}/summary_report.txt
echo "Vector Size | 1 proc -> 2 procs | 1 proc -> 4 procs | 1 proc -> 8 procs | 1 proc -> 16 procs" >> ${RESULTS_DIR}/summary_report.txt
echo "-------------------------------------------------------------------------------------------" >> ${RESULTS_DIR}/summary_report.txt

# Извлекаем времена для разных размеров
for size in 1000000 5000000 10000000 25000000 50000000
do
    # Время для 1 процесса
    time_1=$(grep "vector_size=${size}" ${RESULTS_DIR}/output_1procs.txt 2>/dev/null | grep "PARALLEL" | awk '{print $6}')
    
    if [ ! -z "$time_1" ]; then
        speedup_line="${size}"
        
        for procs in 2 4 8 16
        do
            time_n=$(grep "vector_size=${size}" ${RESULTS_DIR}/output_${procs}procs.txt 2>/dev/null | grep "PARALLEL" | awk '{print $6}')
            
            if [ ! -z "$time_n" ] && [ "$time_1" != "0" ] && [ "$time_n" != "0" ]; then
                speedup=$(echo "scale=2; $time_1 / $time_n" | bc)
                speedup_line="${speedup_line} | ${speedup}x"
            else
                speedup_line="${speedup_line} | -"
            fi
        done
        
        echo "$speedup_line" >> ${RESULTS_DIR}/summary_report.txt
    fi
done

echo -e "\n========================================="
echo "Job completed!"
echo "Results saved in: ${RESULTS_DIR}"
echo -e "\nQuick summary:"
tail -30 ${RESULTS_DIR}/summary_report.txt

# Копируем основные результаты
cp ${RESULTS_DIR}/summary_report.txt mpi_dot_summary.txt

echo -e "\nTo view full results:"
echo "cat ${RESULTS_DIR}/summary_report.txt"
echo "========================================="

exit 0
