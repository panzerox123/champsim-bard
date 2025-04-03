#!/bin/sh

# BASELINE_LRU:
python3 scripts/run_all_workloads.py baseline_open baseline "--dram-page-policy 0" > commands.out
python3 scripts/run_all_workloads.py baseline_close baseline "--dram-page-policy 1" >> commands.out

# WCACHE_LRU:
python3 scripts/run_all_workloads.py wcache_open wcache "--dram-page-policy 0" >> commands.out
python3 scripts/run_all_workloads.py wcache_close wcache "--dram-page-policy 1" >> commands.out

# IDEAL LRU:
#python3 scripts/run_all_workloads.py ideal baseline "--dram-page-policy 2 --dram-ideal-wlp" >> commands.out
