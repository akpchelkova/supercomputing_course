#!/bin/bash
#SBATCH -N 4
#SBATCH -n 32
#SBATCH -t 00:10:00
#SBATCH --mem=1G
#SBATCH -J "topo_32p_4n"
#SBATCH -o "topology_results_20251115_221930/slurm_32p_4n.out"

echo "=== MPI Topology Benchmark ==="
echo "Processes: 32, Nodes: 4"
echo "Start time: $(date)"
echo "Host: $(hostname)"

# Информация о распределении процессов
if [ $SLURM_PROCID -eq 0 ]; then
    echo "SLURM nodes: $SLURM_JOB_NODELIST"
    echo "Processes per node: $SLURM_TASKS_PER_NODE"
fi

# Запуск программы
mpirun ./topology_benchmark

echo "End time: $(date)"
echo "================================="
