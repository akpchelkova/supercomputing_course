#!/bin/bash
#SBATCH --job-name=min_max
#SBATCH --output=min_max_%j.out
#SBATCH --error=min_max_%j.err
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

# Загружаем модули
echo -e "\nLoading modules..."
module purge
module load gcc/9
module load openmpi

echo -e "\nLoaded modules:"
module list

echo -e "\nCompiler info:"
which mpicc
mpicc --version

# Переменные
EXECUTABLE="min_max"
SOURCE_FILE="min_max.c"
RESULTS_DIR="results_${SLURM_JOB_ID}"

# Создаем директорию для результатов
mkdir -p ${RESULTS_DIR}

# Компилируем программу
echo -e "\nCompiling MPI program..."
mpicc -O3 -o ${EXECUTABLE} ${SOURCE_FILE} -lm
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi
echo "Compilation successful."

# Проверяем, что исполняемый файл создан
ls -lh ./${EXECUTABLE}

echo -e "\nRunning experiments..."
echo "========================================="

# Максимальное число процессов для тестирования
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
        
        # Проверяем успешность выполнения
        if [ $? -eq 0 ]; then
            echo "Run completed successfully."
        else
            echo "WARNING: Run with ${PROC_COUNT} processes failed."
        fi
        
        # Извлекаем и показываем основные результаты
        echo "Timing results:"
        grep -E "(PARALLEL:|SEQUENTIAL:)" ${RESULTS_DIR}/output_${PROC_COUNT}procs.txt | tail -5
    fi
done

# Создаем сводный отчет
echo -e "\nGenerating summary report..."
echo "=========================================" > ${RESULTS_DIR}/summary_report.txt
echo "MPI Min/Max Performance Summary" >> ${RESULTS_DIR}/summary_report.txt
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

echo -e "\n========================================="
echo "Job completed!"
echo "Results saved in: ${RESULTS_DIR}"
echo -e "\nQuick summary:"
cat ${RESULTS_DIR}/summary_report.txt | tail -50

# Копируем основные результаты в файл вывода
cp ${RESULTS_DIR}/summary_report.txt mpi_minmax_summary.txt

echo -e "\nTo view full results:"
echo "cat ${RESULTS_DIR}/summary_report.txt"
echo "========================================="

exit 0
