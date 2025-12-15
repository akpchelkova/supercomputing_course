#!/bin/bash
echo "=== Компиляция программ ==="

# Компилируем band.c
mpicc -O3 -lm band.c -o band
echo "band.c скомпилирован -> band"

# Компилируем cannon.c (если он отдельный файл cannon.c)
mpicc -O3 -lm cannon.c -o cannon
echo "cannon.c скомпилирован -> cannon"

# ИЛИ если cannon код в том же файле что и band - переименуй файлы:
# cp band.c cannon.c  # и исправь в cannon.c алгоритм на Кэннона
# mpicc -O3 -lm cannon.c -o cannon

echo "Готово!"
