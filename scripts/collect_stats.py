'''
    author: Suhas Vittal
'''

from sys import argv
from collections import defaultdict
import os

SUITES = ['spec2017', 'ligra', 'stream', 'google']

def read_stat_from_line(line: str, start_string: str, end_string: str):
    left = line.find(start_string) + len(start_string)
    if end_string is None:
        right = len(line)
    else:
        right = line.find(end_string)
    return line[left:right]

def collect_stats(build: str, output_file: str):
    wr = open(output_file, 'w')
    wr.write('Workload,IPC,MPKI,WPKI,Write-Read-Ratio,Write RBHR,Write BLP\n')

    for suite in SUITES:
        data_folder = f'out/{build}/{suite}'

        for f in os.listdir(data_folder):
            if not f.endswith('out'):
                continue
            
            name = f[:f.find('.out')]
            data_file = f'{data_folder}/{f}'
            
            print(name)

            cpu_stats = defaultdict(dict)
            dram_stats = {}

            # READ FILE:
            with open(data_file, 'r') as rd:
                # Skip lines until we reach RoI stats
                line = rd.readline().strip()
                while line != 'Region of Interest Statistics':
                    line = rd.readline().strip()

                while 'MSHR_MERGE' not in line:
                    # Get IPC per core:
                    if 'CPU' in line and 'IPC' in line: 
                        cpuid = int(read_stat_from_line(line, 'CPU', 'cumulative'))
                        ipc = float(read_stat_from_line(line, 'IPC:', 'instructions'))
                        inst = int(read_stat_from_line(line, 'instructions:', 'cycles'))

                        cpu_stats[cpuid]['ipc'] = ipc
                        cpu_stats[cpuid]['inst'] = inst
                    line = rd.readline().strip()

                # Get LLC MPKI per core:
                while line != 'DRAM Statistics':
                    if 'LLC' in line and 'TOTAL' in line:
                        cpuid = int(read_stat_from_line(line, 'cpu', '->LLC'))
                        accesses = int(read_stat_from_line(line, 'ACCESS:', 'HIT:'))
                        misses = int(read_stat_from_line(line, 'MISS:', 'MSHR_MERGE'))

                        mpki = (misses * 1000)/cpu_stats[cpuid]['inst']
                        cpu_stats[cpuid]['mpki'] = mpki
                    line = rd.readline().strip()
                    
                # Get number of reads and writes:
                dram_read_reqs, dram_write_reqs = 0, 0
                dram_write_rbhr = []
                dram_wblp = []
                for i in range(2):
                    while f'Channel {i}' not in line:
                        line = rd.readline()
                    read_reqs = int(read_stat_from_line(line, 'READ REQS:', 'WRITE REQS'))
                    write_reqs = int(read_stat_from_line(line, 'WRITE REQS:', None))

                    # Next line contains read commands + read hits:
                    line = rd.readline()
                    read_cmds = int(read_stat_from_line(line, 'READS:', 'READ_HITS'))
                    read_hits = int(read_stat_from_line(line, 'READ_HITS:', None))

                    # Next line contains same for writes:
                    line = rd.readline()
                    write_cmds = int(read_stat_from_line(line, 'WRITES:', 'WRITE HITS'))
                    write_hits = int(read_stat_from_line(line, 'WRITE HITS:', None))

                    line = rd.readline()   # act and pre
                    line = rd.readline()   # queue full
                    line = rd.readline()   # write drains
                    line = rd.readline()   # write imbalance
                    line = rd.readline()   # read occu

                    # Bank level parallelism for writes
                    line = rd.readline()
                    wblp = float(read_stat_from_line(line, 'MEAN WLP:', None))

                    # update dram stats:
                    dram_read_reqs += read_reqs
                    dram_write_reqs += write_reqs
                    dram_write_rbhr.append(0 if write_cmds == 0 else write_hits/write_cmds)
                    dram_wblp.append(wblp)

                dram_stats['read_requests'] = dram_read_reqs
                dram_stats['write_requests'] = dram_write_reqs
                dram_stats['write_rbhr'] = sum(dram_write_rbhr) / len(dram_write_rbhr)
                dram_stats['write_blp'] = sum(dram_wblp) / len(dram_wblp)
                dram_stats['wpki'] = (dram_write_reqs*1000) / sum(cpu_stats[c]['inst'] for c in cpu_stats)
            # Write stats to csv file:
            ipc = len(cpu_stats) / sum(1.0/cpu_stats[cpuid]['ipc'] for cpuid in cpu_stats)
            mpki = len(cpu_stats) / sum(1.0/cpu_stats[cpuid]['mpki'] for cpuid in cpu_stats)
            wpki = dram_stats['wpki']
            write_read_ratio = dram_stats['write_requests'] / dram_stats['read_requests']
            write_rbhr = dram_stats['write_rbhr']
            write_blp = dram_stats['write_blp']
            wr.write(f'{name},{ipc},{mpki},{wpki},{write_read_ratio},{write_rbhr},{write_blp}\n')
        wr.write('\n')
    wr.close() 

build = argv[1]
output_file = argv[2]

collect_stats(build, output_file)
