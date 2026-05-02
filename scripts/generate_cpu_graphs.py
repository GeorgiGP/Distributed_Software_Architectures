#!/usr/bin/env python3
"""
Генерира CPU графики за MPI експериментите.
Използва по-дълги експерименти за по-ясни графики.
"""

import subprocess
import time
import os
import sys
import threading
from collections import defaultdict

# Check for matplotlib
try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("matplotlib not found, will generate text-based charts")

def monitor_cpu_continuous(stop_event, data_store, interval=0.05):
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

                        # Търсим MPI worker процеси
                        if any(prog in cmd for prog in ['static_fine', 'p2p_full', 'p2p_fib', 'p2p_chain']):
                            if cpu > 0:  # Само активни процеси
                                cpu_by_proc[pid] = cpu
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

def run_experiment_with_monitoring(program, num_procs, arg, name):
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
        args=(stop_event, data, 0.05)
    )

    # Стартираме мониторинга
    monitor_thread.start()

    # Стартираме програмата
    start_time = time.time()
    cmd = f"mpirun -np {num_procs} ./{program} {arg}"
    print(f"  Running: {cmd}")

    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate(timeout=300)

    end_time = time.time()

    # Спираме мониторинга
    stop_event.set()
    monitor_thread.join(timeout=2)

    data['start_time'] = start_time
    data['end_time'] = end_time
    data['duration'] = end_time - start_time
    data['output'] = stdout.decode()

    return data

def process_samples(data):
    """Обработва събраните данни за визуализация"""
    if not data['samples']:
        return None

    start_time = data['start_time']

    # Намираме всички уникални PID-ове
    all_pids = set()
    for sample in data['samples']:
        all_pids.update(sample['cpus'].keys())

    # Сортираме PID-ове за консистентно mapping към P0, P1, etc.
    pid_list = sorted(all_pids)
    pid_to_idx = {pid: i for i, pid in enumerate(pid_list)}

    # Създаваме времеви серии
    times = []
    cpu_series = defaultdict(list)

    for sample in data['samples']:
        t = sample['time'] - start_time
        if t >= 0:
            times.append(t)
            for pid in pid_list:
                cpu_series[pid].append(sample['cpus'].get(pid, 0))

    return {
        'times': times,
        'cpu_series': dict(cpu_series),
        'pid_to_idx': pid_to_idx,
        'num_procs': len(pid_list)
    }

def create_matplotlib_graph(processed_data, data, output_path):
    """Създава графика с matplotlib"""
    if not HAS_MATPLOTLIB:
        return False

    times = processed_data['times']
    cpu_series = processed_data['cpu_series']

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    colors = ['#e41a1c', '#377eb8', '#4daf4a', '#984ea3',
              '#ff7f00', '#ffff33', '#a65628', '#f781bf']

    # График 1: CPU по процеси
    for i, (pid, cpus) in enumerate(cpu_series.items()):
        color = colors[i % len(colors)]
        ax1.plot(times, cpus, label=f'P{i}', color=color, linewidth=1.5, alpha=0.8)
        ax1.fill_between(times, 0, cpus, color=color, alpha=0.2)

    ax1.set_ylabel('CPU Usage (%)', fontsize=12)
    ax1.set_title(f"{data['name']} - CPU per Process ({data['num_procs']} processes)", fontsize=14)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, max(110, max(max(cpus) for cpus in cpu_series.values()) * 1.1))

    # График 2: Обобщен CPU
    total_cpu = [sum(cpu_series[pid][i] for pid in cpu_series)
                 for i in range(len(times))]

    ax2.fill_between(times, 0, total_cpu, color='steelblue', alpha=0.6)
    ax2.plot(times, total_cpu, color='darkblue', linewidth=2)

    # Добавяме линия за максимален теоретичен CPU
    max_theoretical = data['num_procs'] * 100
    ax2.axhline(y=max_theoretical, color='red', linestyle='--',
                label=f'Max ({max_theoretical}%)', alpha=0.7)

    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Total CPU Usage (%)', fontsize=12)
    ax2.set_title(f"Total CPU Utilization (Max theoretical: {max_theoretical}%)", fontsize=14)
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, max_theoretical * 1.2)

    # Добавяме статистика
    avg_total = sum(total_cpu) / len(total_cpu) if total_cpu else 0
    efficiency = (avg_total / max_theoretical) * 100 if max_theoretical > 0 else 0

    fig.text(0.02, 0.02,
             f"Duration: {data['duration']:.2f}s | Avg Total CPU: {avg_total:.0f}% | Efficiency: {efficiency:.1f}%",
             fontsize=10, family='monospace')

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  Graph saved: {output_path}")
    return True

def create_comparison_graph(all_data, output_path):
    """Създава сравнителна графика"""
    if not HAS_MATPLOTLIB:
        return False

    fig, axes = plt.subplots(len(all_data), 1, figsize=(14, 4*len(all_data)), sharex=False)

    if len(all_data) == 1:
        axes = [axes]

    colors = ['#e41a1c', '#377eb8', '#4daf4a', '#984ea3',
              '#ff7f00', '#ffff33', '#a65628', '#f781bf']

    max_time = max(d['processed']['times'][-1] if d['processed']['times'] else 0
                   for d in all_data if d.get('processed'))

    for idx, data in enumerate(all_data):
        ax = axes[idx]
        processed = data.get('processed')

        if not processed:
            ax.text(0.5, 0.5, 'No data', ha='center', va='center')
            continue

        times = processed['times']
        cpu_series = processed['cpu_series']

        # Stacked area plot
        bottom = [0] * len(times)
        for i, (pid, cpus) in enumerate(cpu_series.items()):
            color = colors[i % len(colors)]
            ax.fill_between(times, bottom, [b + c for b, c in zip(bottom, cpus)],
                           label=f'P{i}', color=color, alpha=0.7)
            bottom = [b + c for b, c in zip(bottom, cpus)]

        # Max theoretical line
        max_theoretical = data['num_procs'] * 100
        ax.axhline(y=max_theoretical, color='black', linestyle='--',
                  linewidth=2, alpha=0.5)

        ax.set_ylabel('CPU %')
        ax.set_title(f"{data['name']} ({data['duration']:.2f}s)", fontsize=12, fontweight='bold')
        ax.legend(loc='upper right', ncol=data['num_procs'])
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, max_theoretical * 1.3)
        ax.set_xlim(0, max_time * 1.1)

        # Calculate and show efficiency
        total_cpu = [sum(cpu_series[pid][i] for pid in cpu_series) for i in range(len(times))]
        avg_cpu = sum(total_cpu) / len(total_cpu) if total_cpu else 0
        efficiency = (avg_cpu / max_theoretical) * 100

        ax.text(0.98, 0.95, f'Efficiency: {efficiency:.0f}%',
               transform=ax.transAxes, ha='right', va='top',
               fontsize=11, fontweight='bold',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    axes[-1].set_xlabel('Time (seconds)', fontsize=12)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"Comparison graph saved: {output_path}")
    return True

def main():
    os.chdir('/Users/I764233/uni/RSA/src')
    output_dir = '/Users/I764233/uni/RSA/results/graphs'
    os.makedirs(output_dir, exist_ok=True)

    # Експерименти с повече процеси и по-дълго изпълнение
    experiments = [
        ('static_fine', 4, 42, 'Static Balancing (4 proc)'),
        ('p2p_full_fine', 4, 42, 'P2P Work Stealing (4 proc)'),
        ('p2p_fib_decomp', 4, 45, 'P2P Decomposition (4 proc)'),
    ]

    all_results = []

    for program, procs, arg, name in experiments:
        print(f"\n{'='*60}")
        print(f"Experiment: {name}")
        print(f"{'='*60}")

        data = run_experiment_with_monitoring(program, procs, arg, name)
        processed = process_samples(data)
        data['processed'] = processed

        if processed:
            print(f"  Samples collected: {len(data['samples'])}")
            print(f"  Duration: {data['duration']:.2f}s")
            print(f"  Processes detected: {processed['num_procs']}")

            # Индивидуална графика
            graph_path = f"{output_dir}/{program}_{procs}p.png"
            create_matplotlib_graph(processed, data, graph_path)
        else:
            print("  No CPU data collected")

        all_results.append(data)
        print(f"  Output: {data['output'][:200]}...")

    # Сравнителна графика
    print(f"\n{'='*60}")
    print("Creating comparison graph...")
    create_comparison_graph(all_results, f"{output_dir}/comparison_4proc.png")

    print(f"\nAll graphs saved in {output_dir}/")

if __name__ == '__main__':
    main()
