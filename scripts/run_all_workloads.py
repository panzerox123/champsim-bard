from sys import argv
import os

SUITES = ['spec2017', 'ligra', 'stream', 'google']

workload_list = [
    'wrf', 'roms', 'cam4', 'lbm', 'omnetpp', 'bwaves', 'fotonik3d',
    'triangle', 'pagerankdelta', 'radii', 'bc', 'cf', 'mis', 'pagerank', 'bellmanford',
    'scale', 'copy', 'add', 'triad',
    'whiskey', 'charlie', 'merced', 'delta'
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

output_folder_name = argv[1]
build = argv[2]
cmd_options = argv[3]

INST_SIM = 25_000_000
INST_WARMUP = 25_000_000

for suite in SUITES:
    output_folder = f'out/{output_folder_name}/{suite}'
    if not os.path.isdir(output_folder):
        os.system(f'mkdir -p {output_folder}')

#   trace_folder = f'../frost/TRACES/ctf/{suite}'
    trace_folder = f'TRACES/ctf/{suite}'
    traces = [f for f in os.listdir(trace_folder) if f.endswith('.xz') or f.endswith('.gz')]
    for trace in traces:
        name = get_name(trace, suite)
        if name not in workload_list:
            continue
        trace_filepath = f'{trace_folder}/{trace}'

        trace_part = ' '.join([trace_filepath for _ in range(8)])
        print(f'./bin/{build} {trace_part} --warmup-instructions {INST_WARMUP} --simulation-instructions {INST_SIM} {cmd_options} > {output_folder}/{name}.out')
