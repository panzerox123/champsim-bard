#!/bin/sh

#####################################
# BASELINE                          #
#####################################

python3 scripts/collect_stats.py baseline data/baseline.csv
python3 scripts/collect_stats.py baseline_srrip data/baseline_srrip.csv

python3 scripts/collect_stats.py baseline_open data/baseline_open.csv
python3 scripts/collect_stats.py baseline_soft_close data/baseline_soft_close.csv
python3 scripts/collect_stats.py baseline_mop4 data/baseline_mop4.csv

python3 scripts/collect_stats.py baseline_x8 data/baseline_x8.csv

python3 scripts/collect_stats.py baseline_wb32 data/baseline_wb32.csv
python3 scripts/collect_stats.py baseline_wb64 data/baseline_wb64.csv
python3 scripts/collect_stats.py baseline_wb96 data/baseline_wb96.csv
python3 scripts/collect_stats.py baseline_wb128 data/baseline_wb128.csv

#####################################
# BARD                              #
#####################################

python3 scripts/collect_stats.py bard data/bard.csv
python3 scripts/collect_stats.py bard_srrip data/bard_srrip.csv

python3 scripts/collect_stats.py bard_open data/bard_open.csv
python3 scripts/collect_stats.py bard_soft_close data/bard_soft_close.csv

python3 scripts/collect_stats.py bard_x8 data/bard_x8.csv

python3 scripts/collect_stats.py bard_wb32 data/bard_wb32.csv
python3 scripts/collect_stats.py bard_wb64 data/bard_wb64.csv
python3 scripts/collect_stats.py bard_wb96 data/bard_wb96.csv
python3 scripts/collect_stats.py bard_wb128 data/bard_wb128.csv
