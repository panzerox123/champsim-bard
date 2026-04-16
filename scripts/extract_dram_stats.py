import os
import math
import re
import argparse

def calculate_geomean(values):
    if not values:
        return 0
    # Filter out zeros to avoid log errors
    values = [v for v in values if v > 0]
    if not values:
        return 0
    return math.exp(sum(math.log(v) for v in values) / len(values))

def calculate_mean(values):
    if not values:
        return 0
    return sum(values) / len(values)

def extract_stats(filepath):
    """Extract DRAM stats and LLC miss rate from a ChampSim output file.
    
    DRAM stats (READ LATENCY, TIME IN WRITE MODE) appear per-channel under
    'DRAM Statistics'. LLC miss rate is computed from TOTAL ACCESS/MISS lines
    under 'Region of Interest Statistics'.
    """
    read_latencies = []
    write_mode_times = []
    total_llc_access = 0
    total_llc_miss = 0
    in_roi = False
    in_dram = False

    read_lat_pattern = re.compile(r"READ LATENCY:\s+([0-9.e+\-]+)")
    write_mode_pattern = re.compile(r"TIME IN WRITE MODE:\s+([0-9.e+\-]+)")
    # Matches: cpu0->LLC TOTAL  ACCESS: 12345 HIT: 10000 MISS: 2345 ...
    # Also matches LLC0, LLC1, etc. for private LLC configs
    llc_pattern = re.compile(r"cpu\d+->LLC\d*\s+TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)")

    try:
        with open(filepath, 'r') as f:
            for line in f:
                if "Region of Interest Statistics" in line:
                    in_roi = True
                    continue
                if "DRAM Statistics" in line:
                    in_dram = True
                    continue

                if in_roi and not in_dram:
                    match = llc_pattern.search(line)
                    if match:
                        access = int(match.group(1))
                        miss = int(match.group(3))
                        total_llc_access += access
                        total_llc_miss += miss

                if in_dram:
                    match = read_lat_pattern.search(line)
                    if match:
                        read_latencies.append(float(match.group(1)))
                    
                    match = write_mode_pattern.search(line)
                    if match:
                        write_mode_times.append(float(match.group(1)))
    except Exception:
        pass

    result = {}
    if read_latencies:
        result['read_latency'] = calculate_mean(read_latencies)
    if write_mode_times:
        result['time_in_write_mode'] = sum(write_mode_times)
    if total_llc_access > 0:
        result['llc_miss_rate'] = total_llc_miss / total_llc_access
    
    return result

def main():
    parser = argparse.ArgumentParser(description="Extract DRAM stats and LLC miss rate from ChampSim output files.")
    parser.add_argument("--dir", default="out", help="Base directory for output files (default: out)")
    parser.add_argument("--configs", nargs="+", help="List of configuration folder names to focus on")
    parser.add_argument("--baseline", default="baseline", help="Baseline configuration folder name (default: baseline)")
    parser.add_argument("--output", help="File to write the results to")
    
    args = parser.parse_args()

    if not os.path.exists(args.dir):
        print(f"Error: Directory {args.dir} not found.")
        return

    # Data structure: data[workload_path][run_name] = {read_latency, time_in_write_mode, llc_miss_rate}
    data = {}
    configs_found = set()

    for root, dirs, files in os.walk(args.dir):
        rel_path = os.path.relpath(root, args.dir)
        path_parts = rel_path.split(os.sep)
        
        if not path_parts or path_parts[0] == '.':
            continue
            
        run_name = path_parts[0]
        
        # Filter by configs if specified
        if args.configs:
            if run_name not in args.configs:
                continue

        for file in files:
            if file.endswith('.out'):
                filepath = os.path.join(root, file)
                stats = extract_stats(filepath)
                
                if stats:
                    workload_path = os.path.join(*path_parts[1:], file) if len(path_parts) > 1 else file
                    
                    if workload_path not in data:
                        data[workload_path] = {}
                    data[workload_path][run_name] = stats
                    configs_found.add(run_name)

    if not data:
        print("No data found.")
        return

    configs = sorted(list(configs_found))
    if args.baseline in configs:
        configs.remove(args.baseline)
        configs.insert(0, args.baseline)
    
    output_lines = []
    header = f"{'Trace':<30} | {'Algorithm':<40} | {'Avg Read Lat':<14} | {'Ratio':<14} | {'Time in WM':<14} | {'Ratio':<14} | {'LLC Miss Rate':<14} | {'Ratio'}"
    output_lines.append(header)
    output_lines.append("-" * 170)
    
    latency_ratios = {c: [] for c in configs}
    wm_ratios = {c: [] for c in configs}
    mr_ratios = {c: [] for c in configs}

    for wkld in sorted(data.keys()):
        base_stats = data[wkld].get(args.baseline, {})
        base_lat = base_stats.get('read_latency')
        base_wm = base_stats.get('time_in_write_mode')
        base_mr = base_stats.get('llc_miss_rate')
        
        for alg in configs:
            if alg in data[wkld]:
                stats = data[wkld][alg]
                lat = stats.get('read_latency', 0)
                wm = stats.get('time_in_write_mode', 0)
                mr = stats.get('llc_miss_rate', 0)
                
                # Compute ratios (lower is better for all metrics)
                lat_ratio = (lat / base_lat) if base_lat else 0
                wm_ratio = (wm / base_wm) if base_wm else 0
                mr_ratio = (mr / base_mr) if base_mr else 0
                
                if base_lat and alg != args.baseline:
                    latency_ratios[alg].append(lat_ratio)
                if base_wm and alg != args.baseline:
                    wm_ratios[alg].append(wm_ratio)
                if base_mr and alg != args.baseline:
                    mr_ratios[alg].append(mr_ratio)
                
                lat_ratio_str = f"{lat_ratio:.4f}" if base_lat else "N/A"
                wm_ratio_str = f"{wm_ratio:.4f}" if base_wm else "N/A"
                mr_ratio_str = f"{mr_ratio:.4f}" if base_mr else "N/A"
                if alg == args.baseline:
                    lat_ratio_str = "1.0000 (base)"
                    wm_ratio_str = "1.0000 (base)"
                    mr_ratio_str = "1.0000 (base)"
                    
                line = f"{wkld:<30} | {alg:<40} | {lat:<14.4f} | {lat_ratio_str:<14} | {wm:<14.0f} | {wm_ratio_str:<14} | {mr:<14.6f} | {mr_ratio_str}"
                output_lines.append(line)
        output_lines.append("-" * 170)

    # Calculate average ratios
    output_lines.append("\nGeomean Ratios relative to baseline (lower is better):")
    for alg in configs:
        if alg == args.baseline:
            continue
        parts = []
        if latency_ratios[alg]:
            avg_lat = calculate_geomean(latency_ratios[alg])
            parts.append(f"Read Latency: {avg_lat:.4f}x")
        if wm_ratios[alg]:
            avg_wm = calculate_geomean(wm_ratios[alg])
            parts.append(f"Time in Write Mode: {avg_wm:.4f}x")
        if mr_ratios[alg]:
            avg_mr = calculate_geomean(mr_ratios[alg])
            parts.append(f"LLC Miss Rate: {avg_mr:.4f}x")
        if parts:
            output_lines.append(f"  {alg:<40}: {', '.join(parts)}")

    # Output to console
    for line in output_lines:
        print(line)

    # Output to file if specified
    if args.output:
        try:
            with open(args.output, 'w') as f:
                for line in output_lines:
                    f.write(line + '\n')
            print(f"\nResults successfully written to {args.output}")
        except Exception as e:
            print(f"Error writing to file {args.output}: {e}")

if __name__ == "__main__":
    main()
