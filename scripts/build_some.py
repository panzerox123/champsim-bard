import os

BUILDS = [
    # BASELINE
    'baseline_close',
    # BARD
    'bard_close'
]

for b in BUILDS:
    print(f'BUILDING {b} ===============================\n')
    os.system(f'make clean && make configclean && ./config.sh json/{b}.json && make -j8')

