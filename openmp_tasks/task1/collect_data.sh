#!/bin/bash
echo "Сбор данных для отчета..."
echo "Размер массива,Потоки,Время_посл,Время_редукции,Время_критич,Ускорение_редукции,Ускорение_критич" > results.csv

# Тестируем разные размеры массивов при фиксированном количестве потоков
for size in 1000 10000 100000 1000000 5000000; do
    echo "Размер: $size"
    OMP_NUM_THREADS=4 ./min_max $size | awk -v size=$size '
    /Последовательная версия:/ {getline; seq_time=$3}
    /Параллельная версия \(редукция\):/ {getline; red_time=$3; getline; red_speed=$3}
    /Параллельная версия \(критические секции\):/ {getline; crit_time=$3; getline; crit_speed=$3}
    END {
        gsub(/[()x]/, "", red_speed);
        gsub(/[()x]/, "", crit_speed);
        print size ",4," seq_time "," red_time "," crit_time "," red_speed "," crit_speed
    }' >> results.csv
done

echo "Данные сохранены в results.csv"
