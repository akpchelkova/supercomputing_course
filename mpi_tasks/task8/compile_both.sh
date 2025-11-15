#!/bin/bash
module load openmpi

echo "Compiling both versions..."
mpicc -O2 message_exchange.c -o message_exchange
mpicc -O2 message_exchange_sendrecv.c -o message_exchange_sendrecv
echo "Compilation completed!"
