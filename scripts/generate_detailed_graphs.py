#!/usr/bin/env python3
"""
Генерира детайлни CPU графики за MPI експериментите.
По-дълги изпълнения за по-ясни графики.
"""

import subprocess
import time
import os
import threading
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def monitor_cpu_continuous(stop_event, data_store, target_programs, interval=0.02):
    """Мониторира CPU usage в отделен thread"""
    while not stop_event.is_set():
        try:
            result = subprocess.run(
                ['ps', '-A', '-o', 'pid,%cpu,command'],
                capture_output=True, text=True, timeout=1
            )

            timestamp = time.time()
            cpu_by_proc = {}

            for line in result.stdout.strip().split('\n')[1:]:
                parts = line.strip().split(None, 2)
                if len(parts) >= 3:
                    try:
                        pid = int(parts[0])
                        cpu = float(parts[1])
                        cmd = parts[2]

                        # Филтрираме само целевите програми (без mpirun)
                        for prog in target_programs:
                            if prog in cmd and 'mpirun' not in cmd:
                                if cpu > 0:
                                    cpu_by_proc[pid] = cpu
                                break
                    except:
                        pass

            if cpu_by_proc:
                data_store['samples'].append({
                    'time': timestamp,
                    'cpus': dict(cpu_by_proc)
                })

        except Exception as e:
            pass

        time.sleep(interval)

def run_experiment(program, num_procs, arg, name, target_prog):
    """Изпълнява експеримент с мониторинг"""

    data = {
        'name': name,
        'program': program,
        'num_procs': num_procs,
        'samples': []
    }

    stop_event = threading.Event()
    monitor_thread = threading.Thread(
        target=monitor_cpu_continuous,
        args=(stop_event, data, [target_prog], 0.02)
    )

    monitor_thread.start()
    time.sleep(0.1)  # Даваме време на мониторинга да стартира

    start_time = time.time()
    cmd = f"mpirun -np {num_procs} ./{program} {arg}"
    print(f"  Running: {cmd}")

    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate(timeout=300)

    end_time = time.time()

    time.sleep(0.1)  # Даваме време за финални samples
    stop_event.set()
    monitor_thread.join(timeout=2)

    data['start_time'] = start_time
    data['end_time'] = end_time
    data['duration'] = end_time - start_time
    data['output'] = stdout.decode()

    return data

def process_data(data):
    """Обработва данните за визуализация"""
    if not data['samples']:
        return None

    start_time = data['start_time']

    # Намираме всички PID-ове
    all_pids = set()
    for sample in data['samples']:
        all_pids.update(sample['cpus'].keys())

    pid_list = sorted(all_pids)

    times = []
    cpu_series = {pid: [] for pid in pid_list}

    for sample in data['samples']:
        t = sample['time'] - start_time
        if 0 <= t <= data['duration'] + 0.5:
            times.append(t)
            for pid in pid_list:
                cpu_series[pid].append(sample['cpus'].get(pid, 0))

    return {
        'times': times,
        'cpu_series': cpu_series,
        'num_detected': len(pid_list)
    }

def create_single_graph(data, processed, output_path):
    """Създава детайлна графика за един експеримент"""

    fig, axes = plt.subplots(2, 1, figsize=(14, 8))

    times = processed['times']
    cpu_series = processed['cpu_series']
    num_procs = data['num_procs']

    colors = plt.cm.Set1(np.linspace(0, 1, max(num_procs, len(cpu_series))))

    # Горна графика: CPU по процеси (линии)
    ax1 = axes[0]
    for i, (pid, cpus) in enumerate(cpu_series.items()):
        ax1.plot(times, cpus, label=f'Process {i}', color=colors[i],
                linewidth=2, alpha=0.9)

    ax1.axhline(y=100, color='gray', linestyle=':', alpha=0.5, label='100% (1 core)')
    ax1.set_ylabel('CPU Usage per Process (%)', fontsize=12)
    ax1.set_title(f"{data['name']}\nCPU Usage per Process Over Time", fontsize=14, fontweight='bold')
    ax1.legend(loc='upper left', ncol=min(4, len(cpu_series)))
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 120)
    ax1.set_xlim(0, max(times) if times else 1)

    # Долна графика: Stacked area (обща CPU)
    ax2 = axes[1]

    # Prepare stacked data
    stacked_data = np.array([cpu_series[pid] for pid in cpu_series])
    if len(stacked_data) > 0 and len(times) > 0:
        ax2.stackplot(times, stacked_data, labels=[f'P{i}' for i in range(len(cpu_series))],
                     colors=colors[:len(cpu_series)], alpha=0.7)

    # Теоретичен максимум
    max_theoretical = num_procs * 100
    ax2.axhline(y=max_theoretical, color='red', linestyle='--',
               linewidth=2, label=f'Max theoretical ({max_theoretical}%)')

    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Total CPU Usage (%)', fontsize=12)
    ax2.set_title('Cumulative CPU Utilization', fontsize=14)
    ax2.legend(loc='upper left', ncol=min(5, len(cpu_series)+1))
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, max_theoretical * 1.2)
    ax2.set_xlim(0, max(times) if times else 1)

    # Изчисляваме ефективност
    if times and stacked_data.size > 0:
        total_cpu = np.sum(stacked_data, axis=0)
        avg_cpu = np.mean(total_cpu)
        peak_cpu = np.max(total_cpu)
        efficiency = (avg_cpu / max_theoretical) * 100

        stats_text = (f"Duration: {data['duration']:.2f}s\n"
                     f"Avg CPU: {avg_cpu:.0f}% / {max_theoretical}%\n"
                     f"Peak CPU: {peak_cpu:.0f}%\n"
                     f"Efficiency: {efficiency:.1f}%")

        ax2.text(0.98, 0.95, stats_text, transform=ax2.transAxes,
                ha='right', va='top', fontsize=11, family='monospace',
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.9))

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {output_path}")

def create_comparison_figure(all_data, output_path):
    """Създава сравнителна фигура"""

    fig, axes = plt.subplots(1, 3, figsize=(18, 6))

    colors = plt.cm.Set1(np.linspace(0, 1, 8))

    for idx, (data, ax) in enumerate(zip(all_data, axes)):
        processed = data.get('processed')
        if not processed or not processed['times']:
            ax.text(0.5, 0.5, 'No data', ha='center', va='center', fontsize=14)
            ax.set_title(data['name'])
            continue

        times = processed['times']
        cpu_series = processed['cpu_series']
        num_procs = data['num_procs']

        # Stacked area
        stacked_data = np.array([cpu_series[pid] for pid in cpu_series])
        if stacked_data.size > 0:
            ax.stackplot(times, stacked_data,
                        labels=[f'P{i}' for i in range(len(cpu_series))],
                        colors=colors[:len(cpu_series)], alpha=0.8)

        # Max theoretical
        max_theoretical = num_procs * 100
        ax.axhline(y=max_theoretical, color='red', linestyle='--',
                  linewidth=2, alpha=0.7)

        ax.set_xlabel('Time (s)', fontsize=11)
        ax.set_ylabel('CPU %', fontsize=11)
        ax.set_title(f"{data['name']}\n({data['duration']:.2f}s)", fontsize=12, fontweight='bold')
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, max_theoretical * 1.3)

        # Efficiency
        if stacked_data.size > 0:
            total = np.sum(stacked_data, axis=0)
            eff = (np.mean(total) / max_theoretical) * 100
            ax.text(0.95, 0.95, f'Eff: {eff:.0f}%', transform=ax.transAxes,
                   ha='right', va='top', fontsize=14, fontweight='bold',
                   bbox=dict(boxstyle='round', facecolor='white', alpha=0.9))

        if idx == 0:
            ax.legend(loc='upper left', fontsize=9)

    plt.suptitle('CPU Utilization Comparison: Static vs Work Stealing vs Decomposition',
                fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Comparison saved: {output_path}")

def main():
    os.chdir('/Users/I764233/uni/RSA/src')
    output_dir = '/Users/I764233/uni/RSA/results/graphs'
    os.makedirs(output_dir, exist_ok=True)

    # По-дълги експерименти за по-ясни графики
    experiments = [
        ('static_fine', 4, 43, 'Static Balancing', 'static_fine'),
        ('p2p_full_fine', 4, 43, 'P2P Work Stealing', 'p2p_full_fine'),
        ('p2p_fib_decomp', 4, 46, 'P2P Decomposition', 'p2p_fib_decomp'),
    ]

    all_results = []

    for program, procs, arg, name, target in experiments:
        print(f"\n{'='*60}")
        print(f"Experiment: {name}")
        print(f"{'='*60}")

        data = run_experiment(program, procs, arg, name, target)
        processed = process_data(data)
        data['processed'] = processed

        if processed:
            print(f"  Samples: {len(data['samples'])}")
            print(f"  Duration: {data['duration']:.2f}s")
            print(f"  Processes: {processed['num_detected']}")

            create_single_graph(data, processed, f"{output_dir}/{program}_detail.png")
        else:
            print("  No data collected")

        all_results.append(data)

    # Сравнение
    print(f"\n{'='*60}")
    print("Creating comparison...")
    create_comparison_figure(all_results, f"{output_dir}/comparison_detailed.png")

    print(f"\nGraphs saved in {output_dir}/")

if __name__ == '__main__':
    main()
