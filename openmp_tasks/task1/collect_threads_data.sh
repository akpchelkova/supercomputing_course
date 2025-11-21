#!/bin/bash
echo "Сбор данных по зависимости от количества потоков..."
echo "Потоки,Время_редукции,Время_критич,Ускорение_редукции,Ускорение_критич" > threads_results.csv

for threads in 1 2 4 8 16; do
    echo "Потоки: $threads"
    OMP_NUM_THREADS=$threads ./min_max 1000000 | awk -v threads=$threads '
    /Параллельная версия \(редукция\):/ {getline; red_time=$3; getline; red_speed=$3}
    /Параллельная версия \(критические секции\):/ {getline; crit_time=$3; getline; crit_speed=$3}
    END {
        gsub(/[()x]/, "", red_speed);
        gsub(/[()x]/, "", crit_speed);
        print threads "," red_time "," crit_time "," red_speed "," crit_speed
    }' >> threads_results.csv
done

echo "Данные сохранены в threads_results.csv"
