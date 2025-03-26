from sys import argv
import os

SUITES = ['spec2017', 'ligra', 'parsec']

def get_name(filename: str, suite: str) -> str:
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

build = argv[1]
rate = int(argv[2])

INST_SIM = 100_000_000
INST_WARMUP = 25_000_000

for suite in SUITES:
    output_folder = f'out/{build}/{suite}'
    if not os.path.isdir(output_folder):
        os.system(f'mkdir -p {output_folder}')

    trace_folder = f'../frost/TRACES/ctf/{suite}'
    traces = [f for f in os.listdir(trace_folder) if f.endswith('.xz')]
    for trace in traces:
        name = get_name(trace, suite)
        trace_filepath = f'{trace_folder}/{trace}'

        print(f'python scripts/run_ratemode.py {build} {trace_filepath} {rate} {INST_WARMUP} {INST_SIM} > {output_folder}/{name}.out')
