#!/bin/bash

echo "компиляция программы..."
gcc -fopenmp -O2 -o reduction_comparison_advanced reduction_comparison_advanced.c -lm

echo "сбор данных для исследования..."
echo "threads,size,reduction,critical,atomic,locks,local_array,atomic_bad" > results_threads.csv
echo "threads,size,reduction,critical,atomic,locks,local_array,atomic_bad" > results_sizes.csv

echo "1. исследование зависимости от количества потоков (размер = 10M):"
for threads in 1 2 4 8 16; do
    echo "тестирование $threads потоков..."
    # запускаем с разным количеством потоков и сохраняем в csv
    ./reduction_comparison_advanced --threads $threads --size 10000000 --csv >> results_threads.csv
done

echo "2. исследование зависимости от размера массива (потоки = 4):"
for size in 10000 100000 1000000 10000000; do
    echo "тестирование размера $size..."
    # запускаем с разным размером массива и сохраняем в csv
    ./reduction_comparison_advanced --threads 4 --size $size --csv >> results_sizes.csv
done

echo "данные собраны:"
echo "- results_threads.csv: зависимость от потоков"
echo "- results_sizes.csv: зависимость от размера массива"
