#!/bin/sh

# BASELINE_LRU:
python3 scripts/run_all_workloads.py baseline_close baseline_close "--dram-page-policy 1" > commands.out

# BARD_LRU:
python3 scripts/run_all_workloads.py bard_close bard_close "--dram-page-policy 1 --bard-use-utility-counters" >> commands.out

# VWQ
python3 scripts/run_all_workloads.py vwq vwq_clopen "--dram-page-policy 3 --cache-enable-vwq" >> commands.out

# BARD LRU SENS
#python3 scripts/run_all_workloads.py bard_close_proactive_only_no_umon bard_close "--dram-page-policy 1 --bard-only-proactive-writeback --bard-max-lookup 16" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_shadow_only_no_umon bard_close "--dram-page-policy 1 --bard-only-shadow-writeback --bard-max-lookup 16" >> commands.out

#python3 scripts/run_all_workloads.py bard_close_lookup4 bard_close "--dram-page-policy 1 --bard-max-lookup 4" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_lookup8 bard_close "--dram-page-policy 1 --bard-max-lookup 8" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_lookup16 bard_close "--dram-page-policy 1 --bard-max-lookup 16" >> commands.out

# WRITE BUFFER SENS
python3 scripts/run_all_workloads.py baseline_close_wb32 baseline_close_wb32 "--dram-page-policy 1" >> commands.out
python3 scripts/run_all_workloads.py baseline_close_wb64 baseline_close_wb64 "--dram-page-policy 1" >> commands.out
#python3 scripts/run_all_workloads.py baseline_close_wb96 baseline_close_wb96 "--dram-page-policy 1" >> commands.out

python3 scripts/run_all_workloads.py bard_close_wb32 bard_close_wb32 "--dram-page-policy 1 --bard-use-utility-counters" >> commands.out
python3 scripts/run_all_workloads.py bard_close_wb64 bard_close_wb64 "--dram-page-policy 1 --bard-use-utility-counters" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_wb96 bard_close_wb96 "--dram-page-policy 1 --bard-use-utility-counters" >> commands.out

# SAMPLED SETS SENS
#python3 scripts/run_all_workloads.py bard_close_s16 bard_close "--dram-page-policy 1 --bard-sampled-sets 16" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_s32 bard_close "--dram-page-policy 1 --bard-sampled-sets 32" >> commands.out
#python3 scripts/run_all_workloads.py bard_close_s64 bard_close "--dram-page-policy 1 --bard-sampled-sets 64" >> commands.out

