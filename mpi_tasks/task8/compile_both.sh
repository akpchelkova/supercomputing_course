#!/bin/bash
# загружаем модуль openmpi для работы с mpi
module load openmpi

echo "Compiling both versions..."
# компилируем обе версии программы с оптимизацией O2
mpicc -O2 message_exchange.c -o message_exchange
mpicc -O2 message_exchange_sendrecv.c -o message_exchange_sendrecv
echo "Compilation completed!"
