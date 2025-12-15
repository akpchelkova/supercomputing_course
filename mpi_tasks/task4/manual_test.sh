#!/bin/bash
echo "=== РУЧНОЕ ТЕСТИРОВАНИЕ ==="
echo ""

test_config() {
    local proc=$1
    local size=$2
    local algo=$3
    
    echo "--- $algo на $proc процессах, размер $size x $size ---"
    if [ "$algo" == "band" ]; then
        mpirun -np $proc ./band $size
    else
        mpirun -np $proc ./cannon $size
    fi
    echo ""
}

# Тестируем основные конфигурации
test_config 4 1024 band
test_config 4 1024 cannon
test_config 4 2048 band
test_config 4 2048 cannon
test_config 16 4096 band
test_config 16 4096 cannon
