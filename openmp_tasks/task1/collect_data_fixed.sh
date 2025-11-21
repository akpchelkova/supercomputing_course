#!/bin/bash
echo "Сбор данных для отчета..."
echo "Размер массива,Потоки,Время_посл,Время_редукции,Время_критич,Ускорение_редукции,Ускорение_критич" > results_fixed.csv

for size in 1000 10000 100000 1000000 10000000; do
    echo "Размер: $size"
    OMP_NUM_THREADS=4 ./min_max $size 2>&1 | awk -v size=$size '
    /Последовательная версия:/ {
        getline
        if ($1 == "Минимум:") {
            getline
            seq_time = $3
        }
    }
    /Параллельная версия \(редукция\):/ {
        getline
        if ($1 == "Минимум:") {
            getline
            red_time = $3
            getline
            split($3, arr, "x")
            red_speed = arr[1]
        }
    }
    /Параллельная версия \(критические секции\):/ {
        getline
        if ($1 == "Минимум:") {
            getline
            crit_time = $3
            getline
            split($3, arr, "x")
            crit_speed = arr[1]
        }
    }
    END {
        if (seq_time && red_time && crit_time) {
            print size ",4," seq_time "," red_time "," crit_time "," red_speed "," crit_speed
        }
    }' >> results_fixed.csv
done

echo "Данные сохранены в results_fixed.csv"
