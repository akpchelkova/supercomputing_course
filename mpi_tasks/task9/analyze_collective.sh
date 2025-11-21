#!/bin/bash
echo "=== MPI Task 9: Collective Operations Comprehensive Analysis ==="

# Компиляция
echo "Compiling collective operations..."
mpicc -O2 collective_operations.c -o collective_operations -lm

# Очищаем файл результатов
echo "operation,data_size,processes,mpi_time,my_time,ratio" > collective_results.csv

# Расширенное тестирование с разными конфигурациями
echo "1. Testing intra-node communication (4 processes, 1 node)"
sbatch --wait -n 4 -N 1 -J intra_node_collective -o intra_node_%j.out --wrap="mpirun ./collective_operations"

echo "2. Testing inter-node communication (8 processes, 2 nodes)" 
sbatch --wait -n 8 -N 2 -J inter_node_collective -o inter_node_%j.out --wrap="mpirun ./collective_operations"

echo "3. Testing multi-node communication (16 processes, 4 nodes)"
sbatch --wait -n 16 -N 4 -J multi_node_collective -o multi_node_%j.out --wrap="mpirun ./collective_operations"

echo "All collective operations tests completed!"
echo "Generating analysis..."

# Анализ результатов
echo "=== Collective Operations Analysis ==="
python3 << 'EOF'
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Загрузка данных
df = pd.read_csv('collective_results.csv')

# Группировка по операциям и количеству процессов
summary = df.groupby(['operation', 'processes']).agg({
    'ratio': ['mean', 'std', 'min', 'max'],
    'mpi_time': 'mean',
    'my_time': 'mean'
}).round(3)

print("Summary Statistics:")
print(summary)

# Сохранение сводной таблицы
summary.to_csv('collective_summary.csv')
print("\nDetailed summary saved to collective_summary.csv")

# Анализ эффективности по операциям
operations = df['operation'].unique()
process_counts = df['processes'].unique()

print("\nEfficiency Analysis (Ratio > 1 means MPI is faster):")
for op in operations:
    op_data = df[df['operation'] == op]
    avg_ratio = op_data['ratio'].mean()
    best_case = op_data[op_data['ratio'] == op_data['ratio'].max()]
    worst_case = op_data[op_data['ratio'] == op_data['ratio'].min()]
    
    print(f"\n{op}:")
    print(f"  Average ratio: {avg_ratio:.2f}")
    print(f"  Best case: {best_case['ratio'].values[0]:.2f} (size {best_case['data_size'].values[0]}, processes {best_case['processes'].values[0]})")
    print(f"  Worst case: {worst_case['ratio'].values[0]:.2f} (size {worst_case['data_size'].values[0]}, processes {worst_case['processes'].values[0]})")
    
    if avg_ratio > 1:
        print(f"  Verdict: MPI is {avg_ratio:.1f}x faster on average")
    else:
        print(f"  Verdict: Custom implementation is {1/avg_ratio:.1f}x faster on average")
EOF

echo "Analysis completed!"
