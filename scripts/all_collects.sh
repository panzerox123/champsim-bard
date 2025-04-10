#!/bin/sh

python3 scripts/collect_stats.py baseline_close data/baseline_close.csv
python3 scripts/collect_stats.py bard_close data/bard_close.csv
python3 scripts/collect_stats.py vwq data/vwq.csv

python3 scripts/collect_stats.py bard_close_proactive_only_no_umon data/bard_e_close.csv
python3 scripts/collect_stats.py bard_close_shadow_only_no_umon data/bard_c_close.csv

python3 scripts/collect_stats.py bard_close_lookup16 data/bard_close_lookup16.csv
python3 scripts/collect_stats.py bard_close_lookup8 data/bard_close_lookup8.csv
python3 scripts/collect_stats.py bard_close_lookup4 data/bard_close_lookup4.csv
