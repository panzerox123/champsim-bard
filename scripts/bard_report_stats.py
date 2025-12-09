# author: Suhas Vittal
# stat dump -- does not plot files:

from plotting import *

import numpy as np
import math

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

    print(f'write mode time for {x}: real = {real_wm_frac:.4f}, ideal = {ideal_wm_frac:.4f}, ideal speedup = {ideal_speedup:.4f}')

def bard_report_bank_parallelism(x):
    blp = get_stat_for_workloads(data_map[x], 'Write BLP')
    mean_blp = np.mean(list(blp.values()))
    max_blp = max(blp.values())
    print(f'bank parallelism for {x}: {mean_blp:.2f} ({max_blp:.2f})')

def bard_report_max_sync_overheads(x, cores=128, channels=8):
    total_cycles = get_stat_for_workloads(data_map[x], 'Total Cycles')
    sync_msgs = get_stat_for_workloads(data_map[x], 'BARD Sync Messages')
    writebacks = get_stat_for_workloads(data_map[x], 'Write Requests')

    bard_bandwidth = []
    baseline_bandwidth = []
    for workload in total_cycles:
        # each sync msg -- assume takes 9 bits (8 channels x 64 banks = 512 bits)
#       bits_sent = sync_msgs[workload] * 9
        bits_sent = writebacks[workload] * math.log2(channels*64) * cores//8
        cycles = total_cycles[workload]
        # convert to GB/s  -- clock frequency is 4GHz
        gbps = ((bits_sent/8) / (cycles/4e9)) * 1e-9
        bard_bandwidth.append(gbps)
        gbps = 70*writebacks[workload] * cores//8 / (cycles/4e9) * 1e-9
        baseline_bandwidth.append(gbps)

    mean_baseline_bandwidth = len(baseline_bandwidth) / sum(1.0/bw for bw in baseline_bandwidth)
    max_baseline_bandwidth = max(baseline_bandwidth)
    mean_bard_bandwidth = len(bard_bandwidth) / sum(1.0/bw for bw in bard_bandwidth)
    max_bard_bandwidth = max(bard_bandwidth)

    print(f'mean and max bandwidth for {x}: baseline = {mean_baseline_bandwidth:.4f} GB/s, {max_baseline_bandwidth:.4f} GB/s, bard = {mean_bard_bandwidth:.4f} GB/s, {max_bard_bandwidth:.4f} GB/s')

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
    
def bard_report_accuracy(x):
    bard_evicts = get_stat_for_workloads(data_map[x], 'BARD E Evictions')
    bard_cleanses = get_stat_for_workloads(data_map[x], 'BARD C Cleanses')
    bard_bad_decisions = get_stat_for_workloads(data_map[x], 'BARD Redundant Writebacs')   
    bard_total_decisions = {w : bard_evicts[w] + bard_cleanses[w] for w in bard_evicts}

    inaccuracy = []
    for w in bard_evicts:
        inaccuracy.append(bard_bad_decisions[w] / bard_total_decisions[w])

    mean_acc = np.mean(inaccuracy)
    max_acc = max(inaccuracy)
    print(f'accuracy for {x}: mean = {mean_acc:.4f}, max = {max_acc:.4f}')

def bard_report_misses_and_writebacks(x,y):
    x_mpki = get_stat_for_workloads(data_map[x], 'MPKI')  
    x_wpki = get_stat_for_workloads(data_map[x], 'WPKI')  
    y_mpki = get_stat_for_workloads(data_map[y], 'MPKI')  
    y_wpki = get_stat_for_workloads(data_map[y], 'WPKI')

    rel_mpki, rel_wpki = [], []
    for w in x_mpki:
        if w in ['scale', 'add', 'copy', 'triad']:
            continue
        rel_mpki.append(y_mpki[w] / x_mpki[w])
        rel_wpki.append(y_wpki[w] / x_wpki[w])
#       print(f'{w}: mpki = {rel_mpki[-1]:.4f}, wpki = {rel_wpki[-1]:.4f}')

    gmean_mpki = sp.stats.gmean(rel_mpki)
    gmean_wpki = sp.stats.gmean(rel_wpki)
    max_mpki = max(rel_mpki)
    max_wpki = max(rel_wpki)

    print(f'misses and writebacks for {x} vs {y}: mpki = {gmean_mpki:.4f} ({max_mpki:.4f}), wpki = {gmean_wpki:.4f} ({max_wpki:.4f})')

def bard_report_read_write_latency(x):
    read_lat = get_stat_for_workloads(data_map[x], 'Read Latency')
    write_lat = get_stat_for_workloads(data_map[x], 'Write Latency')

    read_lat = list(read_lat.values())
    write_lat = list(write_lat.values())

    mean_read_lat, max_read_lat = np.mean(read_lat), max(read_lat)
    mean_write_lat, max_write_lat = np.mean(write_lat)/1000, max(write_lat)/1000

    print(f'read and write latency for {x}: read = {mean_read_lat:.4f} ({max_read_lat:.4f}), write = {mean_write_lat:.4f} ({max_write_lat:.4f})')

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
bard_report_speedup('baseline_ship', 'baseline_srrip')

#for wbsize in [32, 64, 96, 128]:
#    for p in ['baseline', 'bard']:
#        bard_report_speedup(f'{p}_wb{wbsize}', f'baseline')

print()

bard_report_speedup('baseline_x8', 'baseline')
bard_report_speedup('bard_x8', 'baseline')
bard_report_speedup('bard_x8', 'baseline_x8')

print('------------------------------ WRITE MODE TIME ---------------------------------')

bard_report_write_mode_time('baseline')
bard_report_write_mode_time('bard')
bard_report_write_mode_time('bard_x8')

print('------------------------------ BANK PARALLELISM ---------------------------------')

bard_report_bank_parallelism('baseline')
bard_report_bank_parallelism('bard')

print('------------------------------ MAX SYNC OVERHEADS ---------------------------------')

bard_report_max_sync_overheads('baseline')
bard_report_max_sync_overheads('bard')

print('------------------------------ DRAM POWER PARAMS ---------------------------------')

bard_report_dram_power_params('baseline')  # power = 362.1 mW 
bard_report_dram_power_params('bard')      # power = 384.0 mW
bard_report_dram_power_params('vwq')       # power = 358.1 mW

bard_report_energy_and_edp('baseline', 362.1)
bard_report_energy_and_edp('bard', 384.0)
bard_report_energy_and_edp('vwq', 358.1)


print('------------------------------ BARD ACCURACY ---------------------------------')
bard_report_accuracy('bard')

print('------------------------------ BARD MISSES AND WRITEBACKS ---------------------------------')
bard_report_misses_and_writebacks('baseline', 'bard')
bard_report_misses_and_writebacks('baseline', 'bard_e')
bard_report_misses_and_writebacks('baseline', 'bard_c')


print('------------------------------ BARD WRITE LATENCY ---------------------------------')
bard_report_read_write_latency('baseline')
bard_report_read_write_latency('bard')

##################################################################
##################################################################

def bard_print_eval_table(policy='baseline'):
    """
    Print a LaTeX table with workload statistics organized by suite.

    Args:
        policy (str): The policy to generate the table for (default: 'bard')

    Columns: Suite, Workload, MPKI, WPKI, WBLP (Write BLP), W% (Write Mode Fraction)
    """
    # Define suite-to-workload mapping based on out directory structure
    suite_workloads = {
        'spec2017': ['cam4', 'roms', 'omnetpp', 'bwaves', 'fotonik3d', 'wrf', 'lbm'],
        'ligra': ['triangle', 'cf', 'pagerankdelta', 'mis', 'bc', 'bellmanford', 'pagerank', 'radii'],
        'stream': ['scale', 'copy', 'triad', 'add'],
        'google': ['whiskey', 'charlie', 'merced', 'delta'],
        'mixes': ['mix0', 'mix1', 'mix2', 'mix3', 'mix4', 'mix5']
    }

    # Suite display names and citations
    suite_display = {
        'spec2017': 'SPEC2017',
        'ligra': 'LIGRA~\\cite{shun2013ligrabenchmarks}',
        'stream': 'STREAM~\\cite{mccalpin1995streambenchmarks}',
        'google': 'Google Srv.~\\cite{dynamoriogoogletraces}',
        'mixes': 'Mixes'
    }

    # Get data for the specified policy
    if policy not in data_map:
        print(f"Error: Policy '{policy}' not found in data_map")
        return

    policy_data = data_map[policy]

    # Check which workloads actually exist in the data
    available_workloads = set(policy_data.keys())

    # Filter suite_workloads to only include available workloads
    filtered_suite_workloads = {}
    for suite, workloads in suite_workloads.items():
        filtered_workloads = [w for w in workloads if w in available_workloads]
        if filtered_workloads:
            filtered_suite_workloads[suite] = filtered_workloads

    # Print LaTeX table header
    print("    \\begin{table}[!htb]")
    print("        \\begin{center}")
    print("            \\footnotesize")
    print("            \\caption{Workload Characteristics}")
    print("            \\vspace{-0.1in}")
    print("            \\label{tab:workloads}")
    print("            \\begin{tabular}{|c|c||c|c|c|c|}")
    print("                \\hline")
    print("                Suite & Workload & MPKI & WPKI & WBLP & W\\% \\\\")
    print("                \\hline")
    print("                \\hline")

    # Process each suite
    suite_order = ['spec2017', 'ligra', 'stream', 'google', 'mixes']
    for suite_idx, suite in enumerate(suite_order):
        if suite not in filtered_suite_workloads:
            continue

        workloads = filtered_suite_workloads[suite]

        # Sort workloads by MPKI (least to greatest)
        workload_mpki_pairs = []
        for workload in workloads:
            if workload in policy_data and 'MPKI' in policy_data[workload]:
                mpki = policy_data[workload]['MPKI']
                workload_mpki_pairs.append((workload, mpki))

        # Sort by MPKI
        workload_mpki_pairs.sort(key=lambda x: x[1])

        # Print multirow for suite name
        num_workloads = len(workload_mpki_pairs)
        suite_name = suite_display[suite]
        print(f"                \\multirow{{{num_workloads}}}{{*}}{{{suite_name}}}")
        print()
        print("                    ")

        # Print rows for this suite
        for i, (workload, _) in enumerate(workload_mpki_pairs):
            stats = policy_data[workload]

            # Extract required statistics
            mpki = stats.get('MPKI', 0.0)
            wpki = stats.get('WPKI', 0.0)
            wblp = stats.get('Write BLP', 0.0)  # Write Bank Level Parallelism
            write_pct = stats.get('Write Mode Fraction', 0.0) * 100  # Convert to percentage

            print(f"& {workload:<15} &\t {mpki:>4.1f} &\t {wpki:>4.1f} &\t  {wblp:>4.1f} &\t  {write_pct:>4.1f} \\\\")

        # Add horizontal lines after each suite (except the last one)
        if suite_idx < len([s for s in suite_order if s in filtered_suite_workloads]) - 1:
            print("                ")
            print("                \\hline")
            print("                \\hline")

    # Print LaTeX table footer
    print()
    print("                \\hline")
    print("            \\end{tabular}")
    print("        \\end{center}")
    print("    \\end{table}")

print()
print('------------------------------ LATEX EVAL TABLE ---------------------------------')
#bard_print_eval_table('baseline')
