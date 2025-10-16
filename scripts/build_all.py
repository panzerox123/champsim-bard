import os

BUILDS = [
    'baseline_single_core',
    # BASELINE
    'baseline', 'baseline_srrip', 'baseline_ship',
    # BARD
    'bard', 'bard_srrip', 'bard_ship',
    # VWQ
    'vwq',
    # EAGER WRITEBACK
    'eager_writeback',
    # SENS
    'baseline_wb32', 'baseline_wb64', 'baseline_wb96', 'baseline_wb128',
    'bard_wb32', 'bard_wb64', 'bard_wb96', 'bard_wb128',
    'baseline_16c', 
    'bard_16c'
]

for b in BUILDS:
    print(f'BUILDING {b} ===============================\n')
    os.system(f'make clean && make configclean && ./config.sh json/{b}.json && make -j8')
