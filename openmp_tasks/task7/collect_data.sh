#!/bin/bash

echo "Компиляция программы..."
gcc -fopenmp -O2 -o reduction_comparison_advanced reduction_comparison_advanced.c -lm

echo "Сбор данных для исследования..."
echo "threads,size,reduction,critical,atomic,locks,local_array,atomic_bad" > results_threads.csv
echo "threads,size,reduction,critical,atomic,locks,local_array,atomic_bad" > results_sizes.csv

echo "1. Исследование зависимости от количества потоков (размер = 10M):"
for threads in 1 2 4 8 16; do
    echo "Тестирование $threads потоков..."
    ./reduction_comparison_advanced --threads $threads --size 10000000 --csv >> results_threads.csv
done

echo "2. Исследование зависимости от размера массива (потоки = 4):"
for size in 10000 100000 1000000 10000000; do
    echo "Тестирование размера $size..."
    ./reduction_comparison_advanced --threads 4 --size $size --csv >> results_sizes.csv
done

echo "Данные собраны:"
echo "- results_threads.csv: зависимость от потоков"
echo "- results_sizes.csv: зависимость от размера массива"
