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

def extract_ipcs(filepath):
    ipcs = {}
    in_roi = False
    # Matches the format found under ROI Statistics:
    pattern = re.compile(r"CPU (\d+) cumulative IPC: ([0-9.]+)")
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                if "Region of Interest Statistics" in line:
                    in_roi = True
                    continue
                
                if in_roi:
                    match = pattern.search(line)
                    if match:
                        cpu_id = int(match.group(1))
                        ipc_val = float(match.group(2))
                        # Store in dict to ensure one value per CPU
                        ipcs[cpu_id] = ipc_val
    except Exception:
        pass
    
    return ipcs

def main():
    parser = argparse.ArgumentParser(description="Extract IPCs and calculate geomeans from ChampSim output files.")
    parser.add_argument("--dir", default="out", help="Base directory for output files (default: out)")
    parser.add_argument("--configs", nargs="+", help="List of configuration folder names to focus on (e.g. private_llc_per_core)")
    parser.add_argument("--baseline", default="baseline", help="Baseline algorithm configuration folder name (default: baseline)")
    parser.add_argument("--output", help="File to write the results to")
    
    args = parser.parse_args()

    if not os.path.exists(args.dir):
        print(f"Error: Directory {args.dir} not found.")
        return

    # Data structure: data[workload_path][run_name] = geomean_ipc
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
                ipcs_dict = extract_ipcs(filepath)
                
                if ipcs_dict:
                    ipc_list = list(ipcs_dict.values())
                    gm_ipc = calculate_geomean(ipc_list)
                    
                    workload_path = os.path.join(*path_parts[1:], file) if len(path_parts) > 1 else file
                    
                    if workload_path not in data:
                        data[workload_path] = {}
                    data[workload_path][run_name] = gm_ipc
                    configs_found.add(run_name)

    if not data:
        print("No data found.")
        return

    configs = sorted(list(configs_found))
    if args.baseline in configs:
        configs.remove(args.baseline)
        configs.insert(0, args.baseline)
    
    output_lines = []
    header = f"{'Trace':<30} | {'Algorithm':<40} | {'IPC':<10} | {'Speedup'}"
    output_lines.append(header)
    output_lines.append("-" * 95)
    
    speedups = {c: [] for c in configs}

    for wkld in sorted(data.keys()):
        base_ipc = data[wkld].get(args.baseline)
        
        for alg in configs:
            if alg in data[wkld]:
                ipc = data[wkld][alg]
                speedup = (ipc / base_ipc) if base_ipc else 0
                if base_ipc and alg != args.baseline:
                    speedups[alg].append(speedup)
                
                speedup_str = f"{speedup:.4f}" if base_ipc else "N/A"
                if alg == args.baseline:
                    speedup_str = "1.0000 (base)"
                    
                line = f"{wkld:<30} | {alg:<40} | {ipc:<10.4f} | {speedup_str}"
                output_lines.append(line)
        output_lines.append("-" * 95)

    # Calculate average speedup
    output_lines.append("\nGeomean Speedup relative to baseline:")
    for alg in configs:
        if alg == args.baseline:
            continue
        if speedups[alg]:
            avg_speedup = calculate_geomean(speedups[alg])
            output_lines.append(f"{alg:<40}: {avg_speedup:.4f}x")

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
