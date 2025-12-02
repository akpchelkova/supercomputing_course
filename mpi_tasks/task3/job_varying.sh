#!/bin/sh
# директива для slurm - название задачи для тестирования с разным количеством процессов
#SBATCH -J mpi_message_varying
# директива для slurm - файл для стандартного вывода результатов
#SBATCH -o result_varying_%j.out
# директива для slurm - файл для вывода ошибок
#SBATCH -e error_varying_%j.err

# загружаем модуль openmpi
module load openmpi

# тестируем разное количество процессов чтобы увидеть как масштабируется производительность
for n in 2 4 8 16
do
    echo "=== Running with $n processes ==="
    # запускаем программу с указанным количеством процессов
    mpirun -n $n ./message_exchange
    echo ""
done
