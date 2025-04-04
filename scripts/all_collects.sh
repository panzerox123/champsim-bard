#!/bin/sh

#python3 scripts/collect_stats.py baseline data/baseline.csv
#python3 scripts/collect_stats.py wcache data/wcache.csv

python3 scripts/collect_stats.py baseline_open data/baseline_open.csv
python3 scripts/collect_stats.py baseline_close data/baseline_close.csv

python3 scripts/collect_stats.py wcache_open data/wcache_open.csv
python3 scripts/collect_stats.py wcache_close data/wcache_close.csv
