#!/bin/sh

# BASELINE_LRU:
python3 scripts/run_all_workloads.py baseline baseline "--dram-page-policy 2" > commands.out

# WCACHE_LRU:
python3 scripts/run_all_workloads.py wcache wcache "--dram-page-policy 2" >> commands.out

# IDEAL LRU:
python3 scripts/run_all_workloads.py ideal baseline "--dram-page-policy 2 --dram-ideal-wlp" >> commands.out
