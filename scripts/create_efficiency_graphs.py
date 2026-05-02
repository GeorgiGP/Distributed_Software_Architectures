#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

# Data from real experiments
processes = [1, 2, 4, 8]

# Efficiency data (Ep = Sp / p)
# Scenario 1: Coarse tasks (32)
static_coarse_Ep = [1.00, 0.73, 0.47, 0.14]  # Sp: 1.00, 1.46, 1.87, 1.15
chain_coarse_Ep = [1.00, 0.76, 0.49, 0.19]   # Sp: 1.00, 1.52, 1.97, 1.49
full_coarse_Ep = [1.00, 0.79, 0.52, 0.24]    # Sp: 1.00, 1.58, 2.07, 1.89

# Scenario 2: Fine tasks (128)
static_fine_Ep = [1.00, 0.78, 0.52, 0.17]    # Sp: 1.00, 1.56, 2.08, 1.38
chain_fine_Ep = [1.00, 0.81, 0.55, 0.22]     # Sp: 1.00, 1.62, 2.18, 1.79
full_fine_Ep = [1.00, 0.85, 0.61, 0.30]      # Sp: 1.00, 1.70, 2.44, 2.42

# Scenario 3: Decomposition (fib 45, threshold 30)
chain_decomp_Ep = [1.00, 0.98, 0.65, 0.57]   # Sp: 1.00, 1.96, 2.62, 4.58
full_decomp_Ep = [1.00, 0.98, 0.91, 0.79]    # Sp: 1.00, 1.95, 3.65, 6.33

# Graph 1: Efficiency comparison - all scenarios at p=8
fig1, ax1 = plt.subplots(figsize=(10, 6))

scenarios = ['Едри задачи\n(32)', 'Ситни задачи\n(128)', 'Декомпозиция\n(fib 45)']
x = np.arange(len(scenarios))
width = 0.25

static_Ep_p8 = [static_coarse_Ep[3], static_fine_Ep[3], None]
chain_Ep_p8 = [chain_coarse_Ep[3], chain_fine_Ep[3], chain_decomp_Ep[3]]
full_Ep_p8 = [full_coarse_Ep[3], full_fine_Ep[3], full_decomp_Ep[3]]

# Plot bars
bars1 = ax1.bar(x[:2] - width, [static_Ep_p8[0], static_Ep_p8[1]], width, label='Статично', color='#808080')
bars2 = ax1.bar(x - 0, chain_Ep_p8, width, label='Chain', color='#4472C4')
bars3 = ax1.bar(x + width, full_Ep_p8, width, label='Full Graph', color='#70AD47')

ax1.set_ylabel('Ефективност (Ep)', fontsize=12)
ax1.set_xlabel('Сценарий', fontsize=12)
ax1.set_title('Сравнение на ефективността при p=8', fontsize=14, fontweight='bold')
ax1.set_xticks(x)
ax1.set_xticklabels(scenarios)
ax1.legend(loc='upper left')
ax1.set_ylim(0, 1.0)
ax1.axhline(y=0.5, color='red', linestyle='--', alpha=0.5, label='50% ефективност')
ax1.grid(axis='y', alpha=0.3)

# Add value labels on bars
for bars in [bars1, bars2, bars3]:
    for bar in bars:
        if bar.get_height() > 0:
            ax1.annotate(f'{bar.get_height():.0%}',
                        xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 3), textcoords="offset points",
                        ha='center', va='bottom', fontsize=9)

plt.tight_layout()
plt.savefig('/Users/I764233/uni/RSA/docs/efficiency_comparison_p8.png', dpi=150)
print("Saved: efficiency_comparison_p8.png")

# Graph 2: Efficiency vs Number of Processes (Decomposition focus)
fig2, ax2 = plt.subplots(figsize=(10, 6))

ax2.plot(processes, chain_decomp_Ep, 'o-', linewidth=2, markersize=8, label='Chain + Декомп.', color='#4472C4')
ax2.plot(processes, full_decomp_Ep, 's-', linewidth=2, markersize=8, label='Full Graph + Декомп.', color='#70AD47')
ax2.plot(processes, chain_coarse_Ep, '^--', linewidth=1.5, markersize=6, label='Chain (едри)', color='#4472C4', alpha=0.5)
ax2.plot(processes, full_coarse_Ep, 'v--', linewidth=1.5, markersize=6, label='Full Graph (едри)', color='#70AD47', alpha=0.5)

ax2.set_xlabel('Брой процеси (p)', fontsize=12)
ax2.set_ylabel('Ефективност (Ep)', fontsize=12)
ax2.set_title('Ефективност при различен брой процеси', fontsize=14, fontweight='bold')
ax2.set_xticks(processes)
ax2.set_ylim(0, 1.1)
ax2.axhline(y=0.5, color='red', linestyle='--', alpha=0.5)
ax2.legend(loc='lower left')
ax2.grid(alpha=0.3)

# Annotate key points
ax2.annotate('79%', xy=(8, 0.79), xytext=(8.3, 0.85), fontsize=10, fontweight='bold', color='#70AD47')
ax2.annotate('57%', xy=(8, 0.57), xytext=(8.3, 0.50), fontsize=10, fontweight='bold', color='#4472C4')

plt.tight_layout()
plt.savefig('/Users/I764233/uni/RSA/docs/efficiency_vs_processes.png', dpi=150)
print("Saved: efficiency_vs_processes.png")

# Graph 3: Speedup comparison with decomposition
fig3, ax3 = plt.subplots(figsize=(10, 6))

# Speedup data
chain_decomp_Sp = [1.00, 1.96, 2.62, 4.58]
full_decomp_Sp = [1.00, 1.95, 3.65, 6.33]
chain_coarse_Sp = [1.00, 1.52, 1.97, 1.49]
full_coarse_Sp = [1.00, 1.58, 2.07, 1.89]
ideal_Sp = [1, 2, 4, 8]

ax3.plot(processes, ideal_Sp, 'k--', linewidth=1, label='Идеално (Sp=p)', alpha=0.5)
ax3.plot(processes, chain_decomp_Sp, 'o-', linewidth=2, markersize=8, label='Chain + Декомп.', color='#4472C4')
ax3.plot(processes, full_decomp_Sp, 's-', linewidth=2, markersize=8, label='Full Graph + Декомп.', color='#70AD47')
ax3.plot(processes, chain_coarse_Sp, '^--', linewidth=1.5, markersize=6, label='Chain (едри)', color='#4472C4', alpha=0.5)
ax3.plot(processes, full_coarse_Sp, 'v--', linewidth=1.5, markersize=6, label='Full Graph (едри)', color='#70AD47', alpha=0.5)

ax3.set_xlabel('Брой процеси (p)', fontsize=12)
ax3.set_ylabel('Ускорение (Sp)', fontsize=12)
ax3.set_title('Ускорение при различен брой процеси', fontsize=14, fontweight='bold')
ax3.set_xticks(processes)
ax3.set_yticks([1, 2, 3, 4, 5, 6, 7, 8])
ax3.legend(loc='upper left')
ax3.grid(alpha=0.3)

# Annotate key points
ax3.annotate('Sp=6.33', xy=(8, 6.33), xytext=(7.2, 7), fontsize=10, fontweight='bold', color='#70AD47',
            arrowprops=dict(arrowstyle='->', color='#70AD47', alpha=0.7))
ax3.annotate('Sp=4.58', xy=(8, 4.58), xytext=(7.2, 3.5), fontsize=10, fontweight='bold', color='#4472C4',
            arrowprops=dict(arrowstyle='->', color='#4472C4', alpha=0.7))

plt.tight_layout()
plt.savefig('/Users/I764233/uni/RSA/docs/speedup_comparison.png', dpi=150)
print("Saved: speedup_comparison.png")

print("\nAll graphs created successfully!")
