#!/bin/sh

rm commands.out
touch commands.out

python3 scripts/run_all_workloads.py baseline_single_core baseline_single_core "--dram-page-policy 2" >> commands.out

#####################################
# BASELINE                          #
#####################################

python3 scripts/run_all_workloads.py baseline baseline "--dram-page-policy 2" >> commands.out

# repl sensitivity
python3 scripts/run_all_workloads.py baseline_srrip baseline_srrip "--dram-page-policy 2" >> commands.out
python3 scripts/run_all_workloads.py baseline_ship baseline_ship "--dram-page-policy 2" >> commands.out

# x8 sensitivity
python3 scripts/run_all_workloads.py baseline_x8 baseline "--dram-page-policy 2 --dram-use-x8-write-timing" >> commands.out

# write buffer sensitivity
python3 scripts/run_all_workloads.py baseline_wb32 baseline_wb32 "--dram-page-policy 2" >> commands.out
python3 scripts/run_all_workloads.py baseline_wb64 baseline_wb64 "--dram-page-policy 2" >> commands.out
python3 scripts/run_all_workloads.py baseline_wb96 baseline_wb96 "--dram-page-policy 2" >> commands.out
python3 scripts/run_all_workloads.py baseline_wb128 baseline_wb128 "--dram-page-policy 2" >> commands.out

# 16 core sensitivity
python3 scripts/run_all_workloads.py baseline_16c baseline_16c "--dram-page-policy 2" >> commands.out

#####################################
# BARD                              #
#####################################

python3 scripts/run_all_workloads.py bard bard "--dram-page-policy 2 --bard-mode 0" >> commands.out

python3 scripts/run_all_workloads.py bard_e bard "--dram-page-policy 2 --bard-mode 1" >> commands.out
python3 scripts/run_all_workloads.py bard_c bard "--dram-page-policy 2 --bard-mode 2" >> commands.out

# repl sensitivity
python3 scripts/run_all_workloads.py bard_srrip bard_srrip "--dram-page-policy 2 --bard-mode 0" >> commands.out
python3 scripts/run_all_workloads.py bard_ship bard_ship "--dram-page-policy 2 --bard-mode 0" >> commands.out

# x8 sensitivity
python3 scripts/run_all_workloads.py bard_x8 bard "--dram-page-policy 2 --bard-mode 0 --dram-use-x8-write-timing" >> commands.out

# write buffer sensitivity
python3 scripts/run_all_workloads.py bard_wb32 bard_wb32 "--dram-page-policy 2 --bard-mode 0" >> commands.out
python3 scripts/run_all_workloads.py bard_wb64 bard_wb64 "--dram-page-policy 2 --bard-mode 0" >> commands.out
python3 scripts/run_all_workloads.py bard_wb96 bard_wb96 "--dram-page-policy 2 --bard-mode 0" >> commands.out
python3 scripts/run_all_workloads.py bard_wb128 bard_wb128 "--dram-page-policy 2 --bard-mode 0" >> commands.out

# 16 core sensitivity
python3 scripts/run_all_workloads.py bard_16c bard_16c "--dram-page-policy 2 --bard-mode 0" >> commands.out

#####################################
# OTHER                             #
#####################################

python3 scripts/run_all_workloads.py eager eager_writeback "--dram-page-policy 2 --cache-enable-eager-writeback" >> commands.out
python3 scripts/run_all_workloads.py vwq vwq "--dram-page-policy 2 --cache-enable-vwq" >> commands.out

