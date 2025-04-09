import os

BUILDS = [
    # BASELINE
    'baseline_close',
    # BARD
    'bard_close',
    # VWQ
#   'vwq_clopen',
    # SENS
#   'baseline_close_wb32', 'baseline_close_wb64', 'baseline_close_wb96',
#   'bard_close_wb32', 'bard_close_wb64', 'bard_close_wb96'
]

for b in BUILDS:
    print(f'BUILDING {b} ===============================\n')
    os.system(f'make clean && make configclean && ./config.sh json/{b}.json && make -j8')
