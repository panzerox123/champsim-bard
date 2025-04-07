#!/bin/sh

python3 scripts/collect_stats.py baseline_close data/baseline_close.csv
python3 scripts/collect_stats.py wlru_close data/wlru_close.csv

python3 scripts/collect_stats.py baseline_soft_close data/baseline_soft_close.csv
python3 scripts/collect_stats.py wlru_soft_close data/wlru_soft_close.csv
