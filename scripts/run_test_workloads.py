from sys import argv
import os

build = argv[1]
rate = int(argv[2])

INST_SIM = 100_000_000
INST_WARMUP = 25_000_000

TRACE_LIST = [
        '619.lbm_s-2676B.champsimtrace.xz',
        '620.omnetpp_s-141B.champsimtrace.xz',
        '623.xalancbmk_s-592B.champsimtrace.xz',
        '649.fotonik3d_s-10881B.champsimtrace.xz',
        '607.cactuBSSN_s-2421B.champsimtrace.xz'
] 

# Verify that all traces exist:
for trace in TRACE_LIST:
    trace_file = f'../frost/TRACES/ctf/spec2017/{trace}'
    if not os.path.exists(trace_file):
        print(f'trace {trace} does not exist')
        exit(1)

for trace in TRACE_LIST:
    trace_file = f'../frost/TRACES/ctf/spec2017/{trace}'
    name = trace[:trace.find('_s')]
    os.system(f'python scripts/run_ratemode.py {build} {trace_file} {rate} {INST_WARMUP} {INST_SIM} > out/{build}_{name}.out &')
