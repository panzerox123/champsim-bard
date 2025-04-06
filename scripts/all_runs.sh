#!/bin/sh

# BASELINE_LRU:
#python3 scripts/run_all_workloads.py baseline_close baseline_close "--dram-page-policy 1" >> commands.out
#python3 scripts/run_all_workloads.py baseline_soft_close baseline_close "--dram-page-policy 2" >> commands.out

# WCACHE_LRU:
python3 scripts/run_all_workloads.py wlru_close wlru_close "--dram-page-policy 1 --bard-use-bitvector" >> commands.out
python3 scripts/run_all_workloads.py wlru_soft_close wlru_close "--dram-page-policy 2 --bard-use-bitvector" >> commands.out
