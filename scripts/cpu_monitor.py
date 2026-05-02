#!/usr/bin/env python3
"""
CPU Monitor - записва CPU натоварването по време на MPI експерименти
и генерира графики.
"""

import subprocess
import time
import sys
import os
import json
from datetime import datetime
from pathlib import Path

def get_cpu_usage_per_core():
    """Взима CPU usage за всяко ядро чрез top на macOS"""
    try:
        result = subprocess.run(
            ['ps', '-A', '-o', '%cpu,command'],
            capture_output=True, text=True, timeout=1
        )

        # Търсим MPI процеси
        cpu_per_process = []
        for line in result.stdout.strip().split('\n')[1:]:
            parts = line.strip().split(None, 1)
            if len(parts) >= 2:
                cpu = float(parts[0])
                cmd = parts[1]
                if 'static_fine' in cmd or 'p2p_full' in cmd or 'p2p_fib' in cmd or 'p2p_chain' in cmd:
                    cpu_per_process.append(cpu)

        return cpu_per_process
    except:
        return []

def get_total_cpu():
    """Взима общо CPU usage"""
    try:
        result = subprocess.run(
            ['top', '-l', '1', '-n', '0'],
            capture_output=True, text=True, timeout=2
        )
        for line in result.stdout.split('\n'):
            if 'CPU usage' in line:
                # CPU usage: 12.5% user, 8.3% sys, 79.2% idle
                parts = line.split(',')
                user = float(parts[0].split(':')[1].strip().replace('%', '').split()[0])
                sys_cpu = float(parts[1].strip().replace('%', '').split()[0])
                return user + sys_cpu
    except:
        pass
    return 0

def monitor_experiment(program, num_procs, arg, duration_limit=120):
    """Мониторира CPU по време на експеримент"""

    data = {
        'program': program,
        'num_procs': num_procs,
        'arg': arg,
        'timestamps': [],
        'cpu_per_process': [],
        'total_cpu': []
    }

    # Стартираме MPI програмата
    cmd = f"mpirun -np {num_procs} ./{program} {arg}"
    print(f"Starting: {cmd}")

    start_time = time.time()
    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Мониторираме докато работи
    sample_interval = 0.1  # 100ms

    while proc.poll() is None:
        elapsed = time.time() - start_time

        if elapsed > duration_limit:
            proc.terminate()
            print("Timeout!")
            break

        cpu_procs = get_cpu_usage_per_core()
        total = sum(cpu_procs) if cpu_procs else get_total_cpu()

        data['timestamps'].append(round(elapsed, 2))
        data['cpu_per_process'].append(cpu_procs)
        data['total_cpu'].append(round(total, 1))

        time.sleep(sample_interval)

    # Финални данни
    stdout, stderr = proc.communicate()
    data['output'] = stdout.decode()
    data['duration'] = round(time.time() - start_time, 3)

    return data

def generate_gnuplot_script(data_files, output_file, title):
    """Генерира gnuplot скрипт"""
    script = f"""
set terminal png size 1200,400 enhanced font 'Arial,12'
set output '{output_file}'
set title '{title}'
set xlabel 'Време (секунди)'
set ylabel 'CPU Usage (%)'
set grid
set key outside right
set yrange [0:*]

plot """

    plots = []
    for i, df in enumerate(data_files):
        plots.append(f"'{df}' using 1:2 with lines lw 2 title 'P{i}'")

    script += ", \\\n     ".join(plots)
    return script

def save_data_for_gnuplot(data, output_dir):
    """Записва данни във формат за gnuplot"""
    os.makedirs(output_dir, exist_ok=True)

    files = []
    num_procs = data['num_procs']

    # Създаваме файл за всеки процес
    for p in range(num_procs):
        filename = f"{output_dir}/cpu_p{p}.dat"
        with open(filename, 'w') as f:
            f.write("# Time CPU\n")
            for i, t in enumerate(data['timestamps']):
                cpus = data['cpu_per_process'][i]
                cpu_val = cpus[p] if p < len(cpus) else 0
                f.write(f"{t} {cpu_val}\n")
        files.append(filename)

    # Общ файл
    total_file = f"{output_dir}/cpu_total.dat"
    with open(total_file, 'w') as f:
        f.write("# Time TotalCPU\n")
        for i, t in enumerate(data['timestamps']):
            f.write(f"{t} {data['total_cpu'][i]}\n")
    files.append(total_file)

    return files

def create_ascii_chart(data, width=60, height=15):
    """Създава ASCII графика на CPU usage"""
    timestamps = data['timestamps']
    total_cpu = data['total_cpu']

    if not timestamps or not total_cpu:
        return "No data collected"

    max_cpu = max(total_cpu) if total_cpu else 100
    max_cpu = max(max_cpu, 100)  # Минимум 100% скала

    # Resample to width
    if len(timestamps) > width:
        step = len(timestamps) // width
        timestamps = timestamps[::step][:width]
        total_cpu = total_cpu[::step][:width]

    chart = []
    chart.append(f"CPU Usage over Time - {data['program']} ({data['num_procs']} processes)")
    chart.append(f"Duration: {data['duration']:.2f}s, Max CPU: {max_cpu:.0f}%")
    chart.append("")

    # Y-axis labels and chart
    for row in range(height, -1, -1):
        threshold = (row / height) * max_cpu
        line = f"{threshold:5.0f}% |"

        for cpu in total_cpu:
            if cpu >= threshold:
                line += "█"
            else:
                line += " "

        chart.append(line)

    # X-axis
    chart.append("       +" + "-" * len(total_cpu))

    # Time labels
    if timestamps:
        start = f"{timestamps[0]:.1f}s"
        end = f"{timestamps[-1]:.1f}s"
        padding = len(total_cpu) - len(start) - len(end)
        chart.append(f"        {start}{' ' * padding}{end}")

    return "\n".join(chart)

def create_per_process_chart(data, width=60, height=12):
    """Създава ASCII графика за всеки процес"""
    timestamps = data['timestamps']
    cpu_per_proc = data['cpu_per_process']
    num_procs = data['num_procs']

    if not timestamps or not cpu_per_proc:
        return "No data collected"

    # Extract per-process data
    proc_data = [[] for _ in range(num_procs)]
    for cpus in cpu_per_proc:
        for p in range(num_procs):
            proc_data[p].append(cpus[p] if p < len(cpus) else 0)

    # Resample
    if len(timestamps) > width:
        step = len(timestamps) // width
        timestamps = timestamps[::step][:width]
        for p in range(num_procs):
            proc_data[p] = proc_data[p][::step][:width]

    charts = []

    for p in range(num_procs):
        chart = []
        max_cpu = max(proc_data[p]) if proc_data[p] else 100
        max_cpu = max(max_cpu, 100)
        avg_cpu = sum(proc_data[p]) / len(proc_data[p]) if proc_data[p] else 0

        chart.append(f"P{p}: avg={avg_cpu:.0f}%")

        # Compact chart (3 rows)
        for row in [2, 1, 0]:
            threshold = (row / 2) * max_cpu * 0.9
            line = "  |"
            for cpu in proc_data[p]:
                if cpu >= threshold:
                    line += "█"
                elif cpu >= threshold * 0.5:
                    line += "▄"
                else:
                    line += " "
            chart.append(line)

        charts.append("\n".join(chart))

    return "\n\n".join(charts)


def main():
    os.chdir('/Users/I764233/uni/RSA/src')

    experiments = [
        ('static_fine', 4, 40, 'Статично балансиране'),
        ('p2p_full_fine', 4, 40, 'P2P Work Stealing'),
        ('p2p_fib_decomp', 4, 45, 'P2P Декомпозиция'),
    ]

    results = []

    for program, procs, arg, name in experiments:
        print(f"\n{'='*60}")
        print(f"Експеримент: {name}")
        print(f"{'='*60}")

        data = monitor_experiment(program, procs, arg)
        data['name'] = name
        results.append(data)

        # Показваме ASCII графика
        print("\n" + create_ascii_chart(data))
        print("\nПо процеси:")
        print(create_per_process_chart(data))
        print("\nOutput:", data['output'][:500] if data['output'] else "N/A")

    # Записваме JSON
    output_dir = '/Users/I764233/uni/RSA/results'
    os.makedirs(output_dir, exist_ok=True)

    with open(f'{output_dir}/cpu_data.json', 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\nДанни записани в {output_dir}/cpu_data.json")

    return results

if __name__ == '__main__':
    main()
