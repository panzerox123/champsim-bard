#!/bin/sh

# BASELINE_LRU:
python3 scripts/run_all_workloads.py baseline_open baseline_open "--dram-page-policy 0" > commands.out
python3 scripts/run_all_workloads.py baseline_close baseline_close "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py baseline_soft_close baseline_close "--dram-page-policy 2" >> commands.out

# WCACHE_LRU:
#python3 scripts/run_all_workloads.py wlru_open wlru_open "--dram-page-policy 0" >> commands.out
python3 scripts/run_all_workloads.py wlru_close wlru_close "--dram-page-policy 1" >> commands.out

# IDEAL LRU:
#python3 scripts/run_all_workloads.py ideal baseline "--dram-page-policy 2 --dram-ideal-wlp" >> commands.out
