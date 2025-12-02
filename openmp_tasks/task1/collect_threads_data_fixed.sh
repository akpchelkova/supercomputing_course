#!/bin/bash
echo "сбор данных по зависимости от количества потоков..."
echo "потоки,время_редукции,время_критич,ускорение_редукции,ускорение_критич" > threads_results_fixed.csv

# улучшенная версия сбора данных по потокам с обработкой ошибок
for threads in 1 2 4 8 16; do
    echo "потоки: $threads"
    # запускаем программу и перенаправляем stderr в stdout для надежного парсинга
    OMP_NUM_THREADS=$threads ./min_max 1000000 2>&1 | awk -v threads=$threads '
    # улучшенный awk скрипт с проверкой наличия данных
    /параллельная версия \(редукция\):/ {
        getline
        if ($1 == "минимум:") {  # проверяем что получили правильную строку
            getline
            red_time = $3
            getline
            split($3, arr, "x")  # разделяем строку ускорения по символу x
            red_speed = arr[1]   # берем только числовое значение
        }
    }
    /параллельная версия \(критические секции\):/ {
        getline
        if ($1 == "минимум:") {  # проверяем что получили правильную строку
            getline
            crit_time = $3
            getline
            split($3, arr, "x")  # разделяем строку ускорения по символу x
            crit_speed = arr[1]   # берем только числовое значение
        }
    }
    END {
        # записываем только если все данные получены
        if (red_time && crit_time) {
            print threads "," red_time "," crit_time "," red_speed "," crit_speed
        }
    }' >> threads_results_fixed.csv
done

echo "данные сохранены в threads_results_fixed.csv"
