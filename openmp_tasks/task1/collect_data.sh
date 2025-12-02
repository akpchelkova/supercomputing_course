#!/bin/bash
echo "сбор данных для отчета..."
echo "размер массива,потоки,время_посл,время_редукции,время_критич,ускорение_редукции,ускорение_критич" > results.csv

# тестируем разные размеры массивов при фиксированном количестве потоков (4)
for size in 1000 10000 100000 1000000 5000000; do
    echo "размер: $size"
    OMP_NUM_THREADS=4 ./min_max $size | awk -v size=$size '
    # awk скрипт для парсинга всех трех версий из вывода программы
    /последовательная версия:/ {getline; seq_time=$3}
    /параллельная версия \(редукция\):/ {getline; red_time=$3; getline; red_speed=$3}
    /параллельная версия \(критические секции\):/ {getline; crit_time=$3; getline; crit_speed=$3}
    END {
        # очищаем строки от лишних символов
        gsub(/[()x]/, "", red_speed);
        gsub(/[()x]/, "", crit_speed);
        # записываем данные в csv формате
        print size ",4," seq_time "," red_time "," crit_time "," red_speed "," crit_speed
    }' >> results.csv
done

echo "данные сохранены в results.csv"
