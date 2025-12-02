#!/bin/bash
echo "сбор данных по зависимости от количества потоков..."
echo "потоки,время_редукции,время_критич,ускорение_редукции,ускорение_критич" > threads_results.csv

# перебираем разное количество потоков для тестирования
for threads in 1 2 4 8 16; do
    echo "потоки: $threads"
    # запускаем программу с указанным количеством потоков и размером массива 1000000
    OMP_NUM_THREADS=$threads ./min_max 1000000 | awk -v threads=$threads '
    # awk скрипт для парсинга вывода программы
    /параллельная версия \(редукция\):/ {getline; red_time=$3; getline; red_speed=$3}
    /параллельная версия \(критические секции\):/ {getline; crit_time=$3; getline; crit_speed=$3}
    END {
        # очищаем строки от лишних символов (скобок и x)
        gsub(/[()x]/, "", red_speed);
        gsub(/[()x]/, "", crit_speed);
        # записываем данные в csv формате
        print threads "," red_time "," crit_time "," red_speed "," crit_speed
    }' >> threads_results.csv
done

echo "данные сохранены в threads_results.csv"
