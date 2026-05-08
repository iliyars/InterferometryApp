"""
ПОЛНЫЙ СКРИПТ: Объединение → Сохранение → Визуализация с номерами
Python версия скрипта MATLAB
"""

import os
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path
import numpy as np

# ========== ПЕРЕМЕННЫЕ ДЛЯ НАСТРОЙКИ ==========
dir_name = "22"
numbering_type = "every_nth"  # "all", "every_nth", "global", "none"
point_step = 5                # Показывать каждую N-ую точку
font_size = 9                 # Размер шрифта номеров

# ==============================================

print("═" * 60)
print("ПОЛНЫЙ ПРОЦЕСС: Объединение → Сохранение → Визуализация")
print("═" * 60 + "\n")

# ЭТАП 1: ОБЪЕДИНЕНИЕ ДАННЫХ
print("ЭТАП 1: Объединение CSV файлов")
print("─" * 60)

folder_path = os.path.join('test_images', dir_name)

# Проверяем, существует ли папка
if not os.path.isdir(folder_path):
    print(f"❌ Ошибка: Папка не найдена: {folder_path}")
    exit(1)

# Ищем файлы
file_pattern = os.path.join(folder_path, 'line_*_points.csv')
files = sorted([f for f in os.listdir(folder_path)
                if f.startswith('line_') and f.endswith('_points.csv')])

if not files:
    print(f"❌ Ошибка: Файлы не найдены в папке: {folder_path}")
    exit(1)

print(f"Найдено файлов: {len(files)}\n")

# Инициализируем таблицы
all_data = []
file_summary = []

# Читаем каждый файл
for filename in files:
    try:
        filepath = os.path.join(folder_path, filename)
        df = pd.read_csv(filepath)

        x = df['x'].values
        y = df['y'].values
        line_id = df['line_id'].iloc[0]
        num_points = len(df)

        # Создаём данные линии
        for i, (xi, yi) in enumerate(zip(x, y)):
            all_data.append({
                'line_id': line_id,
                'x': xi,
                'y': yi
            })

        file_summary.append({
            'line_id': line_id,
            'points': num_points
        })

        print(f"✓ Линия {line_id:2d} ({num_points:3d} точек)")

    except Exception as e:
        print(f"✗ Ошибка в {filename}: {str(e)}")

# Преобразуем в DataFrame
all_data_df = pd.DataFrame(all_data)
file_summary_df = pd.DataFrame(file_summary)

print(f"\nВсего загружено: {len(all_data_df)} строк\n")

# ЭТАП 2: СОХРАНЕНИЕ ДАННЫХ
print("ЭТАП 2: Сохранение данных")
print("─" * 60)

# Создаём папку dir_name если её нет
script_dir = os.path.dirname(os.path.abspath(__file__))
save_folder = os.path.join(script_dir, dir_name)
if not os.path.isdir(save_folder):
    os.makedirs(save_folder)
    print(f"✓ Папка создана: {dir_name}")
else:
    print(f"✓ Папка существует: {dir_name}")

# Сохраняем основной файл
data_filename = f'combined_lines_{dir_name}.csv'
data_filepath = os.path.join(save_folder, data_filename)
all_data_df.to_csv(data_filepath, index=False)
print(f"✓ Сохранены данные: {data_filename}")

# Сохраняем сводку
summary_filename = f'lines_summary_{dir_name}.csv'
summary_filepath = os.path.join(save_folder, summary_filename)
file_summary_df.to_csv(summary_filepath, index=False)
print(f"✓ Сохранена сводка: {summary_filename}\n")

# ЭТАП 3: ВИЗУАЛИЗАЦИЯ
print("ЭТАП 3: Построение графика")
print("─" * 60)

# Создаём фигуру
fig, ax = plt.subplots(figsize=(12, 8))
ax.grid(True, alpha=0.3)
ax.set_aspect('equal')

# Получаем цвета
colors = plt.cm.viridis(np.linspace(0, 1, len(files)))

global_point_number = 1

# Для каждого файла
for i, filename in enumerate(files):
    try:
        filepath = os.path.join(folder_path, filename)
        df = pd.read_csv(filepath)

        line_id = df['line_id'].iloc[0]
        x = df['x'].values
        y = df['y'].values

        # Рисуем линию
        ax.plot(x, y, 'o-', color=colors[i],
                linewidth=1.5, markersize=3,
                label=f'Line {line_id}')

        # ========== ДОБАВЛЯЕМ НОМЕР К КАЖДОЙ ТОЧКЕ ==========

        if numbering_type == "all":
            for point_idx in range(len(x)):
                ax.text(x[point_idx], y[point_idx],
                       f'{point_idx + 1}',
                       fontsize=font_size, fontweight='bold',
                       color=colors[i],
                       ha='center', va='bottom')
                global_point_number += 1

        elif numbering_type == "every_nth":
            for point_idx in range(len(x)):
                if (point_idx + 1) % point_step == 0 or point_idx == len(x) - 1 or point_idx == 0:
                    ax.text(x[point_idx], y[point_idx],
                           f'{point_idx + 1}',
                           fontsize=font_size, fontweight='bold',
                           color=colors[i],
                           ha='center', va='bottom')
                global_point_number += 1

        elif numbering_type == "global":
            for point_idx in range(len(x)):
                if (point_idx + 1) % point_step == 0 or point_idx == len(x) - 1 or point_idx == 0:
                    ax.text(x[point_idx], y[point_idx],
                           f'{global_point_number}',
                           fontsize=font_size, fontweight='bold',
                           color=colors[i],
                           ha='center', va='bottom')
                global_point_number += 1
        else:
            global_point_number += len(x)

        # ========== ПОДПИСЬ ЛИНИИ В КОНЦЕ ==========
        last_x = x[-1]
        last_y = y[-1]
        ax.text(last_x, last_y, f'  Line {line_id}',
               fontsize=10, fontweight='bold',
               color=colors[i],
               va='center', ha='left')

    except Exception as e:
        print(f"✗ Ошибка: {str(e)}")

# Оформление графика
ax.set_xlabel('X (пиксели)', fontsize=12)
ax.set_ylabel('Y (пиксели)', fontsize=12)
ax.set_title(f'Lines from folder {dir_name} - {len(files)} files (numbering: {numbering_type})',
            fontsize=14, fontweight='bold')
ax.legend(loc='center left', bbox_to_anchor=(1, 0.5))

# Инвертируем Y ось (как на изображении)
ax.invert_yaxis()

# Сохраняем график
plot_filename = f'lines_plot_{dir_name}.png'
plt.savefig(os.path.join(save_folder, plot_filename), dpi=150, bbox_inches='tight')
print(f"✓ График сохранён: {plot_filename}")

plt.show()

# ИТОГОВАЯ ИНФОРМАЦИЯ
print("\n" + "─" * 60)
print("ИТОГО:")
print("─" * 60 + "\n")

print(f"📁 Папка для сохранения: {dir_name}/")
print(f"📄 Основной файл:       {data_filename}")
print(f"📊 Файл сводки:         {summary_filename}")
print(f"📈 Тип нумерации:       {numbering_type}")
print(f"📍 Загружено точек:     {global_point_number - 1}")

print("\n" + "═" * 60)
print("✓ ЗАВЕРШЕНО!")
print("═" * 60)
