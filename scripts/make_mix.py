import os
import random

SUITES = ['spec2017', 'ligra', 'stream', 'google']

def get_name(filename: str, suite: str) -> str:
    left, right = 0, filename.find('.champsim')
    if suite == 'spec2017':
        left = len('6xx.')
        right = filename.find('_s')
    elif suite == 'ligra':
        left = len('ligra_')
        right = filename.find('.com')
    elif suite == 'parsec':
        left = len('parsec_2.1.')
        right = filename.find('.simlarge')
    return filename[left:right].lower()

workloads = [
    'wrf', 'roms', 'cam4', 'lbm', 'omnetpp', 'bwaves', 'fotonik3d',
    'triangle', 'pagerankdelta', 'radii', 'bc', 'cf', 'mis', 'pagerank', 'bellmanford',
    'whiskey', 'charlie', 'merced', 'delta'
]

# only refresh the workload list once all workloads have been selected once
remaining = workloads.copy()
for k in range(6):
    print(f'-----------------mix {k}---------------------')
    visited = set()
    for i in range(8):
        if len(remaining) == 0:
            remaining = workloads.copy()
        j = random.randint(0,len(remaining)-1)
        while remaining[j] in visited:
            j = random.randint(0,len(remaining)-1)
        print(remaining[j])
        visited.add(remaining[j])
        del remaining[j]
