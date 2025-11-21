#!/bin/bash
echo "Сбор данных по зависимости от количества потоков..."
echo "Потоки,Время_редукции,Время_критич,Ускорение_редукции,Ускорение_критич" > threads_results_fixed.csv

for threads in 1 2 4 8 16; do
    echo "Потоки: $threads"
    OMP_NUM_THREADS=$threads ./min_max 1000000 2>&1 | awk -v threads=$threads '
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
        if (red_time && crit_time) {
            print threads "," red_time "," crit_time "," red_speed "," crit_speed
        }
    }' >> threads_results_fixed.csv
done

echo "Данные сохранены в threads_results_fixed.csv"
