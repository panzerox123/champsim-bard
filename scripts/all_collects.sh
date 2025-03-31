#!/bin/sh

python3 scripts/collect_stats.py baseline data/baseline.csv
python3 scripts/collect_stats.py wcache data/wcache.csv
python3 scripts/collect_stats.py wcache_soft data/wcache_soft.csv
python3 scripts/collect_stats.py ideal data/ideal.csv
