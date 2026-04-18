import matplotlib.pyplot as plt
import numpy as np

# Geomean ratios relative to baseline from private_llc_results.txt
configs = ['private_llc_per_core_bard_lru', 'private_llc_per_core_bard_lru_baws', 'private_llc_per_core_baws']
labels = ['BARD LRU', 'BARD LRU+BAWS', 'BAWS']

read_latency = [1.0326, 1.0107, 0.9733]
time_in_write_mode = [1.1283, 1.0146, 0.8765]
llc_miss_rate = [0.9835, 0.9952, 0.9986]

# Color palette
colors = ['#4C72B0', '#DD8452', '#55A868']

fig, axes = plt.subplots(1, 3, figsize=(16, 5))

# --- Read Latency ---
ax = axes[0]
bars = ax.bar(labels, read_latency, color=colors, edgecolor='black', linewidth=0.6, width=0.5)
ax.axhline(y=1.0, color='red', linestyle='--', linewidth=1, label='Baseline (1.0x)')
ax.set_ylabel('Ratio relative to Baseline', fontsize=11)
ax.set_title('Read Latency', fontsize=13, fontweight='bold')
ax.set_ylim(min(min(read_latency), 1.0) - 0.03, max(max(read_latency), 1.0) + 0.03)
for bar, val in zip(bars, read_latency):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.003,
            f'{val:.4f}x', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.legend(fontsize=9)

# --- Time in Write Mode ---
ax = axes[1]
bars = ax.bar(labels, time_in_write_mode, color=colors, edgecolor='black', linewidth=0.6, width=0.5)
ax.axhline(y=1.0, color='red', linestyle='--', linewidth=1, label='Baseline (1.0x)')
ax.set_ylabel('Ratio relative to Baseline', fontsize=11)
ax.set_title('Time in Write Mode', fontsize=13, fontweight='bold')
ax.set_ylim(min(min(time_in_write_mode), 1.0) - 0.05, max(max(time_in_write_mode), 1.0) + 0.05)
for bar, val in zip(bars, time_in_write_mode):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.005,
            f'{val:.4f}x', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.legend(fontsize=9)

# --- LLC Miss Rate ---
ax = axes[2]
bars = ax.bar(labels, llc_miss_rate, color=colors, edgecolor='black', linewidth=0.6, width=0.5)
ax.axhline(y=1.0, color='red', linestyle='--', linewidth=1, label='Baseline (1.0x)')
ax.set_ylabel('Ratio relative to Baseline', fontsize=11)
ax.set_title('LLC Miss Rate', fontsize=13, fontweight='bold')
ax.set_ylim(min(min(llc_miss_rate), 1.0) - 0.01, max(max(llc_miss_rate), 1.0) + 0.01)
for bar, val in zip(bars, llc_miss_rate):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.001,
            f'{val:.4f}x', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.legend(fontsize=9)

fig.suptitle('Private LLC Per-Core: Geomean Ratios Relative to Baseline (lower is better)',
             fontsize=14, fontweight='bold', y=1.02)
plt.tight_layout()
plt.savefig('private_llc_graphs.png', dpi=200, bbox_inches='tight')
plt.show()
print("Saved to private_llc_graphs.png")
