# author: Suhas Vittal
# stat dump -- does not plot files:

from plotting import *

import numpy as np

POLICY_LIST = ['baseline', 
               'baseline_16c', 
               'baseline_srrip',
               'baseline_ship',
               'baseline_wb32',
               'baseline_wb64',
               'baseline_wb96',
               'baseline_wb128',
               'baseline_x8',
               'bard',
               'bard_e',
               'bard_c',
               'bard_16c',
               'bard_srrip',
               'bard_ship',
               'bard_wb32',
               'bard_wb64',
               'bard_wb96',
               'bard_wb128',
               'bard_x8',
               'eager',
               'vwq']

data_map = {p : read_csv_data(f'data/{p}.csv') for p in POLICY_LIST}

def bard_report_speedup(x, y):
    gm, mx =  report_gmean_and_max_speedup(data_map[x], data_map[y])
    gm, mx = (gm-1)*100 , (mx-1)*100
    print(f'speedup of {x} over {y}: mean = {gm:.1f}%, max = {mx:.1f}%')

def bard_report_write_mode_time(x):
    real_wm_time = get_stat_for_workloads(data_map[x], 'Write Mode Fraction')
    ideal_wm_time = get_stat_for_workloads(data_map[x], 'Ideal Write Mode Fraction')
    ideal_speedup = get_stat_for_workloads(data_map[x], 'Ideal Speedup')

    # get mean:
    real_wm_frac = len(real_wm_time) / sum(1.0/x for x in real_wm_time.values())
    ideal_wm_frac = len(ideal_wm_time) / sum(1.0/x for x in ideal_wm_time.values())
    ideal_speedup = sp.stats.gmean(list(ideal_speedup.values()))

    print(f'write mode time for {x}: real = {real_wm_frac:.2f}, ideal = {ideal_wm_frac:.2f}, ideal speedup = {ideal_speedup:.2f}')

def bard_report_bank_parallelism(x):
    blp = get_stat_for_workloads(data_map[x], 'Write BLP')
    blp = np.mean(list(blp.values()))
    print(f'bank parallelism for {x}: {blp:.2f}')

def bard_report_max_sync_overheads(x):
    total_cycles = get_stat_for_workloads(data_map[x], 'Total Cycles')
    sync_msgs = get_stat_for_workloads(data_map[x], 'BARD Sync Messages')

    bandwidth = []
    for workload in total_cycles:
        # each sync msg -- assume takes 9 bits (8 channels x 64 banks = 512 bits)
        bits_sent = sync_msgs[workload] * 9
        cycles = total_cycles[workload]
        # convert to GB/s  -- clock frequency is 4GHz
        gbps = ((bits_sent/8) / (cycles/4e9)) * 1e-9
        bandwidth.append(gbps)
    mean_bandwidth = len(bandwidth) / sum(1.0/b for b in bandwidth)
    max_bandwidth = max(bandwidth)
    print(f'max sync overheads for {x}: mean = {mean_bandwidth:.2f} GB/s, max = {max_bandwidth:.2f} GB/s')

def bard_report_dram_power_params(x):
    total_cycles = get_stat_for_workloads(data_map[x], 'Total Cycles')
    reads = get_stat_for_workloads(data_map[x], 'Read Requests')
    writes = get_stat_for_workloads(data_map[x], 'Write Requests')
    acts = get_stat_for_workloads(data_map[x], 'Activates')
    pre = get_stat_for_workloads(data_map[x], 'Precharges')

    time_in_s = []
    read_occu_frac = []
    write_occu_frac = []
    acts_per_ps = []

    for workload in total_cycles:
        tt_ps = total_cycles[workload] * 250
        rf = (reads[workload] * 3300 * 0.5) / tt_ps
        wf = (writes[workload] * 3300 * 0.5) / tt_ps
        aps = (acts[workload] * 0.5) / tt_ps

        time_in_s.append(tt_ps * 1e-12)
        read_occu_frac.append(rf)
        write_occu_frac.append(wf)
        acts_per_ps.append(aps)

    mean_time_in_s = np.mean(time_in_s)
    mean_read_occu_frac = len(read_occu_frac) / sum(1.0/rf for rf in read_occu_frac)
    mean_write_occu_frac = len(write_occu_frac) / sum(1.0/wf for wf in write_occu_frac)
    mean_acts_per_ps = len(acts_per_ps) / sum(1.0/aps for aps in acts_per_ps)

    print(f'dram power params for {x}: read occupancy = {mean_read_occu_frac:.4f}, write occupancy = {mean_write_occu_frac:.4f}, ns per act = {1e-3/mean_acts_per_ps:.4f}, time in s = {mean_time_in_s:.4f}')

def bard_report_energy_and_edp(x, power_in_mw: float):
    total_cycles = get_stat_for_workloads(data_map[x], 'Total Cycles')
    
    energy = []
    edp = []

    for workload in total_cycles:
        tt_ps = total_cycles[workload] * 250
        e = power_in_mw * tt_ps * 1e-12
        energy.append(e)
        edp.append(e * tt_ps * 1e-12)

    mean_energy = np.mean(energy)
    mean_edp = np.mean(edp)
    print(f'energy and edp for {x}: energy = {mean_energy:.4f} mJ, edp = {mean_edp:.4f} mJ*s')

print('------------------------------ SPEEDUP ---------------------------------')

bard_report_speedup('bard', 'baseline')
bard_report_speedup('bard_e', 'baseline')
bard_report_speedup('bard_c', 'baseline')
bard_report_speedup('eager', 'baseline')
bard_report_speedup('vwq', 'baseline')

print()

bard_report_speedup('bard_16c', 'baseline_16c')
bard_report_speedup('bard_srrip', 'baseline_srrip')
bard_report_speedup('bard_ship', 'baseline_ship')

#for wbsize in [32, 64, 96, 128]:
#    for p in ['baseline', 'bard']:
#        bard_report_speedup(f'{p}_wb{wbsize}', f'baseline')

print()

bard_report_speedup('baseline_x8', 'baseline')
bard_report_speedup('bard_x8', 'baseline')

print('------------------------------ WRITE MODE TIME ---------------------------------')

bard_report_write_mode_time('baseline')
bard_report_write_mode_time('bard')
bard_report_write_mode_time('bard_x8')

print('------------------------------ BANK PARALLELISM ---------------------------------')

bard_report_bank_parallelism('baseline')
bard_report_bank_parallelism('bard')

print('------------------------------ MAX SYNC OVERHEADS ---------------------------------')

bard_report_max_sync_overheads('bard')

print('------------------------------ DRAM POWER PARAMS ---------------------------------')

bard_report_dram_power_params('baseline')  # power = 362.1 mW 
bard_report_dram_power_params('bard')      # power = 384.0 mW
bard_report_dram_power_params('vwq')       # power = 358.1 mW

bard_report_energy_and_edp('baseline', 362.1)
bard_report_energy_and_edp('bard', 384.0)
bard_report_energy_and_edp('vwq', 358.1)
