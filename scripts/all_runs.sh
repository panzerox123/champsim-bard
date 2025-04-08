#!/bin/sh

# BASELINE_LRU:
python3 scripts/run_all_workloads.py baseline_close baseline_close "--dram-page-policy 1" > commands.out
python3 scripts/run_all_workloads.py baseline_close_srrip baseline_close_srrip "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py baseline_close_ship baseline_close_ship "--dram-page-policy 1" >> commands.out

# BARD_LRU:
python3 scripts/run_all_workloads.py bard_close bard_close "--dram-page-policy 1" >> commands.out

# BARD LRU SENS
python3 scripts/run_all_workloads.py bard_close_bitvec bard_close "--dram-page-policy 1 --bard-use-bitvector" >> commands.out

python3 scripts/run_all_workloads.py bard_close_lookup2 bard_close "--dram-page-policy 1 --bard-use-bitvector --bard-max-lookup 2" >> commands.out
python3 scripts/run_all_workloads.py bard_close_lookup4 bard_close "--dram-page-policy 1 --bard-use-bitvector --bard-max-lookup 4" >> commands.out
python3 scripts/run_all_workloads.py bard_close_lookup8 bard_close "--dram-page-policy 1 --bard-use-bitvector --bard-max-lookup 8" >> commands.out
python3 scripts/run_all_workloads.py bard_close_lookup16 bard_close "--dram-page-policy 1 --bard-use-bitvector --bard-max-lookup 16" >> commands.out

# WRITE BUFFER SENS
python3 scripts/run_all_workloads.py baseline_close_wb32 baseline_close_wb32 "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py baseline_close_wb64 baseline_close_wb64 "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py baseline_close_wb96 baseline_close_wb96 "--dram-page-policy 1" >> commands.out

python3 scripts/run_all_workloads.py bard_close_wb32 bard_close_wb32 "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py bard_close_wb64 bard_close_wb64 "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py bard_close_wb96 bard_close_wb96 "--dram-page-policy 1" >> commands.out

# SAMPLED SETS SENS
python3 scripts/run_all_workloads.py bard_close_s16 bard_close "--dram-page-policy 1 --bard-sampled-sets 16" >> commands.out
python3 scripts/run_all_workloads.py bard_close_s32 bard_close "--dram-page-policy 1 --bard-sampled-sets 32" >> commands.out
python3 scripts/run_all_workloads.py bard_close_s64 bard_close "--dram-page-policy 1 --bard-sampled-sets 64" >> commands.out

