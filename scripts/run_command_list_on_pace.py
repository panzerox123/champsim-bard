import os
import time

rd = open('commands.out')

joblimit_per_sleep = 196

jobs = 0
lines = rd.readlines()
for line in lines:
    line_parts = line.split('>')
    if '16c' in line:
        t = '72:00:00'
    else:
        t = '24:00:00'
    cmd = f'sbatch -N1 --ntasks-per-node=1 --mem-per-cpu=4G -t{t} --account=gts-mqureshi4-rg -o{line_parts[1].strip()} --wrap=\"{line_parts[0].strip()}\"'

    print(line_parts[1])
    os.system(cmd)

    jobs += 1

print(f'jobs: {jobs}')
rd.close()
