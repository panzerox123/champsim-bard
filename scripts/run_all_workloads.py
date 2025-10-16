from sys import argv
import os

SUITES = ['spec2017', 'ligra', 'stream', 'google', 'mixes']

workload_list = [
    'wrf', 'roms', 'cam4', 'lbm', 'omnetpp', 'bwaves', 'fotonik3d',
    'triangle', 'pagerankdelta', 'radii', 'bc', 'cf', 'mis', 'pagerank', 'bellmanford',
    'scale', 'copy', 'add', 'triad',
    'whiskey', 'charlie', 'merced', 'delta'
]

mix_list = [
    ['cam4', 'omnetpp', 'mis', 'cf', 'merced', 'delta', 'whiskey', 'lbm'],
    ['pagerank', 'bc', 'wrf', 'roms', 'radii', 'bellmanford', 'triangle', 'fotonik3d'],
    ['bwaves', 'pagerankdelta', 'charlie', 'roms', 'whiskey', 'bc', 'triangle', 'delta'],
    ['omnetpp', 'pagerankdelta', 'bwaves', 'cf', 'pagerank', 'bellmanford', 'mis', 'radii'],
    ['wrf', 'merced', 'fotonik3d', 'lbm', 'cam4', 'charlie', 'radii', 'bc'],
    ['lbm', 'roms', 'fotonik3d', 'wrf', 'pagerankdelta', 'delta', 'triangle', 'bwaves']
]

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

# need this map to convert mixes into trace list:
workload_to_trace_map = {}
for suite in SUITES:
    if suite == 'mixes':
        continue
    trace_folder = f'TRACES/ctf/{suite}'
    traces = [f for f in os.listdir(trace_folder) if f.endswith('.xz') or f.endswith('.gz')]
    for trace in traces:
        name = get_name(trace, suite)
        workload_to_trace_map[name] = f'TRACES/ctf/{suite}/{trace}'

output_folder_name = argv[1]
build = argv[2]
cmd_options = argv[3]

INST_SIM = 100_000_000
INST_WARMUP = 25_000_000

# RATEMODE EVALS:
NUM_CORES = 8
if '16c' in build:
    NUM_CORES = 16
if 'single_core' in build:
    NUM_CORES = 1

for suite in SUITES:
    if suite == 'mixes' and NUM_CORES == 1:
        continue
    output_folder = f'out/{output_folder_name}/{suite}'
    if not os.path.isdir(output_folder):
        os.system(f'mkdir -p {output_folder}')

    if suite == 'mixes':
        mixes = [[workload_to_trace_map[w] for w in mix] for mix in mix_list]
        for (i,mix) in enumerate(mixes):
            name = f'mix{i}'
            trace_part = []
            for trace in mix:
                for _ in range(NUM_CORES//8):
                    trace_part.append(trace)
            trace_part = ' '.join(trace_part)
            print(f'./bin/{build} {trace_part} --warmup-instructions {INST_WARMUP} --simulation-instructions {INST_SIM} {cmd_options} > {output_folder}/{name}.out')
    else:
        trace_folder = f'TRACES/ctf/{suite}'
        traces = [f for f in os.listdir(trace_folder) if f.endswith('.xz') or f.endswith('.gz')]
        for trace in traces:
            name = get_name(trace, suite)
            if name not in workload_list:
                continue
            trace_filepath = f'{trace_folder}/{trace}'

            trace_part = ' '.join([trace_filepath for _ in range(NUM_CORES)])
            print(f'./bin/{build} {trace_part} --warmup-instructions {INST_WARMUP} --simulation-instructions {INST_SIM} {cmd_options} > {output_folder}/{name}.out')

