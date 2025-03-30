from sys import argv
import os

exec = argv[1]
trace = argv[2]
rate = int(argv[3])
inst_warmup = int(argv[4])
inst_sim = int(argv[5])
other_args = ' '.join(argv[6:])

trace_part = ' '.join([trace for _ in range(rate)])

print(f'bin/{exec} {trace_part} --warmup-instructions {inst_warmup} --simulation-instructions {inst_sim} {other_args}')
os.system(f'bin/{exec} {trace_part} --warmup-instructions {inst_warmup} --simulation-instructions {inst_sim} {other_args}')
