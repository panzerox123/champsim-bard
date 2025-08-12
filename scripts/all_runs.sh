#!/bin/sh

rm commands.out
touch commands.out

#####################################
# BASELINE                          #
#####################################

#python3 scripts/run_all_workloads.py baseline baseline "--dram-page-policy 2" >> commands.out

# repl sensitivity
#python3 scripts/run_all_workloads.py baseline_srrip baseline_srrip "--dram-page-policy 2" >> commands.out

# x8 sensitivity
#python3 scripts/run_all_workloads.py baseline_x8 baseline "--dram-page-policy 2 --dram-use-x8-write-timing" >> commands.out

# write buffer sensitivity
#python3 scripts/run_all_workloads.py baseline_wb32 baseline_wb32 "--dram-page-policy 2" >> commands.out
#python3 scripts/run_all_workloads.py baseline_wb64 baseline_wb64 "--dram-page-policy 2" >> commands.out
#python3 scripts/run_all_workloads.py baseline_wb96 baseline_wb96 "--dram-page-policy 2" >> commands.out
#python3 scripts/run_all_workloads.py baseline_wb128 baseline_wb128 "--dram-page-policy 2" >> commands.out

# 16 core sensitivity
#python3 scripts/run_all_workloads.py baseline_16c baseline_16c "--dram-page-policy 2" >> commands.out

#####################################
# BARD                              #
#####################################

#python3 scripts/run_all_workloads.py bard bard "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out

#python3 scripts/run_all_workloads.py bard_e bard "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector --bard-only-proactive-writeback" >> commands.out
#python3 scripts/run_all_workloads.py bard_c bard "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector --bard-only-shadow-writeback" >> commands.out

# repl sensitivity
#python3 scripts/run_all_workloads.py bard_srrip bard_srrip "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out

# x8 sensitivity
#python3 scripts/run_all_workloads.py bard_x8 bard "--dram-page-policy 2 --bard-max-lookup 16 --dram-use-x8-write-timing --bard-use-bitvector" >> commands.out

# write buffer sensitivity
#python3 scripts/run_all_workloads.py bard_wb32 bard_wb32 "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_wb64 bard_wb64 "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_wb96 bard_wb96 "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_wb128 bard_wb128 "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out

# lookup Sensitivity
#python3 scripts/run_all_workloads.py bard_restrict2way bard "--dram-page-policy 2 --bard-max-lookup 2 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_restrict4way bard "--dram-page-policy 2 --bard-max-lookup 4 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_restrict8way bard "--dram-page-policy 2 --bard-max-lookup 8 --bard-use-bitvector" >> commands.out
#python3 scripts/run_all_workloads.py bard_restrict12way bard "--dram-page-policy 2 --bard-max-lookup 12 --bard-use-bitvector" >> commands.out

# 16 core sensitivity
#python3 scripts/run_all_workloads.py bard_16c bard_16c "--dram-page-policy 2 --bard-max-lookup 16 --bard-use-bitvector" >> commands.out

#####################################
# BARD-U                            #
#####################################

python3 scripts/run_all_workloads.py bard_u bard "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out

# repl sensitivity
python3 scripts/run_all_workloads.py bard_u_srrip bard_srrip "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out

# x8 sensitivity
python3 scripts/run_all_workloads.py bard_u_x8 bard "--dram-page-policy 2 --dram-use-x8-write-timing --bard-use-bitvector --bard-use-utility-counters" >> commands.out

# write buffer sensitivity
python3 scripts/run_all_workloads.py bard_u_wb32 bard_wb32 "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out
python3 scripts/run_all_workloads.py bard_u_wb64 bard_wb64 "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out
python3 scripts/run_all_workloads.py bard_u_wb96 bard_wb96 "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out
python3 scripts/run_all_workloads.py bard_u_wb128 bard_wb128 "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out

# 16 core sensitivity
python3 scripts/run_all_workloads.py bard_u_16c bard_16c "--dram-page-policy 2 --bard-use-bitvector --bard-use-utility-counters" >> commands.out

#####################################
# OTHER                             #
#####################################

#python3 scripts/run_all_workloads.py eager eager_writeback "--dram-page-policy 2 --cache-enable-eager-writeback" >> commands.out
#python3 scripts/run_all_workloads.py vwq vwq "--dram-page-policy 2 --cache-enable-vwq" >> commands.out

