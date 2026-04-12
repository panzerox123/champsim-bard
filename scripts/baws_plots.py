import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os
import re
from scipy.stats import gmean

run_types = [
    "baseline",
    "bard",
    "private_llc_per_core",
    "private_llc_per_core_bard_lru"
]

workloads = {
    "spec2017": ["lbm"],
    "ligra": ["cf"],
    "mixes": ["mix2", "mix3"]
}

ipcs = {
    "run_type": []
}

blps = {
    "run_type": []
}

bglps = {
    "run_type": []
}

for run_type in run_types:
    ipcs['run_type'].append(run_type)
    blps['run_type'].append(run_type)
    bglps['run_type'].append(run_type)
    for suite in workloads:
        for workload in workloads[suite]:
            if ipcs.get(workload) == None:
                ipcs[workload] = []
            if blps.get(workload) == None:
                blps[workload] = []
            if bglps.get(workload) == None:
                bglps[workload] = []
            output_path = os.path.join("out", run_type, suite, f"{workload}.out");
            if not os.path.exists(output_path):
                raise Exception(f"{output_path} does not exist!")
            f = open(output_path, 'r')
            is_roi = False
            ipc_by_core = []
            blp = []
            bglp = []
            ipc_line_re = re.compile(r"^CPU\s+(\d+)\s+cumulative IPC:\s+([0-9]*\.?[0-9]+)\b")
            blp_line_re = re.compile(r"^\s+MEAN BLP:\s+([0-9]*\.?[0-9]+)\s+BGLP:\s+([0-9]*\.?[0-9]+)")
            for line in f.readlines():
                if "Region of Interest Statistics" in line:
                    is_roi = True
                if not is_roi:
                    continue
                m = ipc_line_re.match(line)
                if m:
                    core = int(m.group(1))
                    ipc = float(m.group(2))
                    ipc_by_core.append(ipc)
                b = blp_line_re.match(line)
                if b:
                    blp.append(float(b.group(1)))
                    bglp.append(float(b.group(2)))
            f.close()
            ipcs[workload].append(gmean(ipc_by_core))
            blps[workload].append(gmean(blp))
            bglps[workload].append(gmean(bglp))

print(ipcs)
print(bglps)
print(blps)

baseline = 0
for suite in workloads:
    for workload in workloads[suite]:
        baseline = 0
        for i in range(len(ipcs[workload])):
            if i == 0:
                baseline = ipcs[workload][i]
            ipcs[workload][i] /= baseline

ipc_df = pd.DataFrame(ipcs)
blp_df = pd.DataFrame(blps)
bglp_df = pd.DataFrame(bglps)

print(ipc_df)

ipc_df_plot = ipc_df.set_index('run_type').T
ipc_df_plot.plot(kind='bar', figsize=(10, 6))
plt.xlabel('Workload')
plt.ylabel('IPC')
plt.title('Normalized IPC by Workload')
plt.legend(title='Run Type')
plt.tight_layout()
plt.show()
plt.savefig("ipcs.png")

blp_df_plot = blp_df.set_index('run_type').T
blp_df_plot.plot(kind='bar', figsize=(10, 6))
plt.xlabel('Workload')
plt.ylabel('BLP')
plt.title('BLP by Workload')
plt.legend(title='Run Type')
plt.tight_layout()
plt.show()
plt.savefig("blp.png")

bglp_df_plot = bglp_df.set_index('run_type').T
bglp_df_plot.plot(kind='bar', figsize=(10, 6))
plt.xlabel('Workload')
plt.ylabel('BGLP')
plt.title('BGLP by Workload')
plt.legend(title='Run Type')
plt.tight_layout()
plt.show()
plt.savefig("bglp.png")




