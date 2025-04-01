'''
    author: Suhas Vittal
'''

from sys import argv
from collections import defaultdict
import os

SUITES = ['spec2017', 'ligra', 'stream']

def collect_stats(build: str, output_file: str):
    wr = open(output_file, 'w')
    wr.write('Workload,IPC,MPKI,Write-Read-Ratio\n')

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
                        left, right = len('CPU'), line.find('cumulative')
                        cpuid = int(line[left:right])

                        left, right = line.find('IPC:') + len('IPC:'), line.find('instructions')
                        ipc = float(line[left:right])

                        left, right = line.find('instructions:') + len('instructions:'), line.find('cycles')
                        inst = int(line[left:right])

                        cpu_stats[cpuid]['ipc'] = ipc
                        cpu_stats[cpuid]['inst'] = inst
                    line = rd.readline().strip()

                # Get LLC MPKI per core:
                while line != 'DRAM Statistics':
                    if 'LLC' in line and 'TOTAL' in line:
                        left, right = len('cpu'), line.find('->LLC')
                        cpuid = int(line[left:right])

                        left, right = line.find('ACCESS:') + len('ACCESS:'), line.find('HIT:')
                        accesses = int(line[left:right])

                        left, right = line.find('MISS:') + len('MISS:'), line.find('MSHR_MERGE')
                        misses = int(line[left:right])

                        mpki = (misses * 1000)/cpu_stats[cpuid]['inst']
                        cpu_stats[cpuid]['mpki'] = mpki
                    line = rd.readline().strip()
                    
                # Get number of reads and writes:
                dram_reads, dram_writes = 0, 0
                for i in range(2):
                    while f'Channel {i}' not in line:
                        line = rd.readline()
                    left, right = line.find('READ REQS:')+len('READ REQS:'), line.find('WRITE REQS:')
                    reads = int(line[left:right])

                    left, right = line.find('WRITE REQS:')+len('WRITE REQS:'), len(line)
                    writes = int(line[left:right])

                    dram_reads += reads
                    dram_writes += writes
                dram_stats['reads'] = dram_reads
                dram_stats['writes'] = dram_writes
            # Write stats to csv file:
            ipc = len(cpu_stats) / sum(1.0/cpu_stats[cpuid]['ipc'] for cpuid in cpu_stats)
            mpki = len(cpu_stats) / sum(1.0/cpu_stats[cpuid]['mpki'] for cpuid in cpu_stats)
            write_read_ratio = dram_stats['writes'] / dram_stats['reads']
            wr.write(f'{name},{ipc},{mpki},{write_read_ratio}\n')
        wr.write('\n')
    wr.close() 

build = argv[1]
output_file = argv[2]

collect_stats(build, output_file)
