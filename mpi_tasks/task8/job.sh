#!/bin/bash
#SBATCH --job-name=mpi_full_comparison
#SBATCH --output=full_comparison_%j.out
#SBATCH --error=full_comparison_%j.err
#SBATCH --time=00:20:00
#SBATCH --partition=gnu
#SBATCH --nodes=2
#SBATCH --ntasks=4
#SBATCH --ntasks-per-node=2
#SBATCH --exclude=g9
#SBATCH --distribution=block:block

module load openmpi
module load gcc/10

mpicc -O3 -o message_exchange message_exchange.c -lm
mpicc -O3 -o message_exchange_sendrecv message_exchange_sendrecv.c -lm

echo "=================================================================================="
echo "FULL MPI COMMUNICATION COMPARISON: Send/Recv vs Sendrecv"
echo "=================================================================================="
echo "Start time: $(date)"
echo "MPI version: $(mpicc --version 2>&1 | head -1)"
echo "Allocated nodes: $SLURM_JOB_NUM_NODES"
echo "Nodes list: $SLURM_JOB_NODELIST"
echo "=================================================================================="

echo ""
echo "TEST 1: Check node distribution and hostnames"
echo "=================================================================================="
srun hostname
echo ""

echo "TEST 2: Single node (intra-node) - Process 0-1 on same node"
echo "=================================================================================="
echo "Separate Send/Recv (intra-node):"
srun -n 2 --nodes=1 ./message_exchange 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""
echo "Combined Sendrecv (intra-node):"
srun -n 2 --nodes=1 ./message_exchange_sendrecv 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""

echo "TEST 3: Cross-node - Process 0 on node1, Process 1 on node2"
echo "=================================================================================="
echo "Separate Send/Recv (cross-node):"
srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""
echo "Combined Sendrecv (cross-node):"
srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange_sendrecv 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""

echo "TEST 4: Multiple pairs (4 processes, 2 nodes)"
echo "=================================================================================="
echo "Separate Send/Recv (2 pairs, 2 nodes):"
srun -n 4 --nodes=2 --ntasks-per-node=2 ./message_exchange 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""
echo "Combined Sendrecv (2 pairs, 2 nodes):"
srun -n 4 --nodes=2 --ntasks-per-node=2 ./message_exchange_sendrecv 2>&1 | grep -E "(Size|^[0-9])" | head -12
echo ""

echo "TEST 5: Detailed performance comparison table"
echo "=================================================================================="
echo "Message Size | Intra-Node Send/Recv | Intra-Node Sendrecv | Cross-Node Send/Recv | Cross-Node Sendrecv"
echo "--------------------------------------------------------------------------------------------------------"

SIZES="1024 4096 16384 65536 262144 1048576"
for SIZE in $SIZES; do
    echo -n "$SIZE | "
    
    # Intra-node
    INTRA_SEP=$(srun -n 2 --nodes=1 ./message_exchange 2>&1 | grep "^$SIZE" | awk '{printf "%.3fms (%.0fMB/s)", $2, $3}')
    INTRA_SEN=$(srun -n 2 --nodes=1 ./message_exchange_sendrecv 2>&1 | grep "^$SIZE" | awk '{printf "%.3fms (%.0fMB/s)", $2, $3}')
    
    # Cross-node
    CROSS_SEP=$(srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange 2>&1 | grep "^$SIZE" | awk '{printf "%.3fms (%.0fMB/s)", $2, $3}')
    CROSS_SEN=$(srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange_sendrecv 2>&1 | grep "^$SIZE" | awk '{printf "%.3fms (%.0fMB/s)", $2, $3}')
    
    echo "$INTRA_SEP | $INTRA_SEN | $CROSS_SEP | $CROSS_SEN"
done

echo ""
echo "TEST 6: Speedup calculation"
echo "=================================================================================="
echo "Message Size | Intra-Node Speedup | Cross-Node Speedup"
echo "--------------------------------------------------------"

for SIZE in 1024 16384 65536 1048576; do
    # Get times
    INTRA_SEP_TIME=$(srun -n 2 --nodes=1 ./message_exchange 2>&1 | grep "^$SIZE" | awk '{print $2}')
    INTRA_SEN_TIME=$(srun -n 2 --nodes=1 ./message_exchange_sendrecv 2>&1 | grep "^$SIZE" | awk '{print $2}')
    
    CROSS_SEP_TIME=$(srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange 2>&1 | grep "^$SIZE" | awk '{print $2}')
    CROSS_SEN_TIME=$(srun -n 2 --nodes=2 --ntasks-per-node=1 ./message_exchange_sendrecv 2>&1 | grep "^$SIZE" | awk '{print $2}')
    
    if [ ! -z "$INTRA_SEP_TIME" ] && [ ! -z "$INTRA_SEN_TIME" ]; then
        INTRA_SPEEDUP=$(echo "scale=2; $INTRA_SEP_TIME / $INTRA_SEN_TIME" | bc)
    else
        INTRA_SPEEDUP="N/A"
    fi
    
    if [ ! -z "$CROSS_SEP_TIME" ] && [ ! -z "$CROSS_SEN_TIME" ]; then
        CROSS_SPEEDUP=$(echo "scale=2; $CROSS_SEP_TIME / $CROSS_SEN_TIME" | bc)
    else
        CROSS_SPEEDUP="N/A"
    fi
    
    echo "$SIZE | $INTRA_SPEEDUP | $CROSS_SPEEDUP"
done

echo ""
echo "End time: $(date)"
echo "=================================================================================="
