#!/usr/bin/env python3
"""
Plotting utility script for ChampSim data analysis.

This script provides functionality to read CSV data files and return structured
dictionaries for easy data manipulation and plotting.
"""

import csv
import os
from typing import Dict, Any, Union


def read_csv_data(csv_file_path: str) -> Dict[str, Dict[str, float]]:
    """
    Read a CSV data file and return a dictionary structure.
    
    Args:
        csv_file_path (str): Path to the CSV file to read
        
    Returns:
        Dict[str, Dict[str, float]]: Dictionary where:
            - Keys are workload names
            - Values are dictionaries mapping stat names to their values
            
    Example:
        {
            'wrf': {
                'IPC': 0.577336773200857,
                'MPKI': 25.40440122931382,
                'WPKI': 7.300981140485283,
                ...
            },
            'roms': {
                'IPC': 1.0697342355003931,
                'MPKI': 13.082756848074627,
                ...
            }
        }
    """
    if not os.path.exists(csv_file_path):
        raise FileNotFoundError(f"CSV file not found: {csv_file_path}")
    
    workload_data = {}
    
    with open(csv_file_path, 'r', newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        
        for row in reader:
            # Skip empty rows
            if not row.get('Workload') or row['Workload'].strip() == '':
                continue
                
            workload_name = row['Workload'].strip()
            stats = {}
            
            # Convert all non-workload columns to float values
            for key, value in row.items():
                if key != 'Workload' and value.strip() != '':
                    try:
                        stats[key] = float(value)
                    except ValueError:
                        # If conversion fails, keep as string
                        stats[key] = value.strip()
            
            workload_data[workload_name] = stats
    
    return workload_data


def read_multiple_csv_files(csv_file_paths: list) -> Dict[str, Dict[str, Dict[str, float]]]:
    """
    Read multiple CSV files and return a nested dictionary structure.
    
    Args:
        csv_file_paths (list): List of paths to CSV files to read
        
    Returns:
        Dict[str, Dict[str, Dict[str, float]]]: Dictionary where:
            - Top-level keys are file names (without extension)
            - Second-level keys are workload names
            - Third-level keys are stat names mapping to their values
            
    Example:
        {
            'baseline': {
                'wrf': {'IPC': 0.577, 'MPKI': 25.404, ...},
                'roms': {'IPC': 1.069, 'MPKI': 13.082, ...}
            },
            'bard': {
                'wrf': {'IPC': 0.593, 'MPKI': 25.357, ...},
                'roms': {'IPC': 1.098, 'MPKI': 13.208, ...}
            }
        }
    """
    all_data = {}
    
    for csv_path in csv_file_paths:
        if not os.path.exists(csv_path):
            print(f"Warning: CSV file not found: {csv_path}")
            continue
            
        # Extract filename without extension as the key
        file_key = os.path.splitext(os.path.basename(csv_path))[0]
        all_data[file_key] = read_csv_data(csv_path)
    
    return all_data


def get_stat_for_workloads(data: Dict[str, Dict[str, float]], stat_name: str) -> Dict[str, float]:
    """
    Extract a specific statistic for all workloads.
    
    Args:
        data (Dict[str, Dict[str, float]]): Data dictionary from read_csv_data()
        stat_name (str): Name of the statistic to extract (e.g., 'IPC', 'MPKI')
        
    Returns:
        Dict[str, float]: Dictionary mapping workload names to the specified stat value
        
    Example:
        get_stat_for_workloads(data, 'IPC') returns:
        {'wrf': 0.577, 'roms': 1.069, 'cam4': 1.406, ...}
    """
    result = {}
    for workload, stats in data.items():
        if stat_name in stats:
            result[workload] = stats[stat_name]
    return result


def get_workload_stats(data: Dict[str, Dict[str, float]], workload_name: str) -> Dict[str, float]:
    """
    Get all statistics for a specific workload.
    
    Args:
        data (Dict[str, Dict[str, float]]): Data dictionary from read_csv_data()
        workload_name (str): Name of the workload
        
    Returns:
        Dict[str, float]: Dictionary of all stats for the specified workload
        
    Example:
        get_workload_stats(data, 'wrf') returns:
        {'IPC': 0.577, 'MPKI': 25.404, 'WPKI': 7.300, ...}
    """
    return data.get(workload_name, {})


def list_available_stats(data: Dict[str, Dict[str, float]]) -> list:
    """
    Get a list of all available statistics in the dataset.
    
    Args:
        data (Dict[str, Dict[str, float]]): Data dictionary from read_csv_data()
        
    Returns:
        list: List of all unique statistic names found in the data
    """
    all_stats = set()
    for workload_stats in data.values():
        all_stats.update(workload_stats.keys())
    return sorted(list(all_stats))


def list_available_workloads(data: Dict[str, Dict[str, float]]) -> list:
    """
    Get a list of all available workloads in the dataset.
    
    Args:
        data (Dict[str, Dict[str, float]]): Data dictionary from read_csv_data()
        
    Returns:
        list: List of all workload names found in the data
    """
    return sorted(list(data.keys()))


# Example usage and testing
if __name__ == "__main__":
    # Example usage
    print("ChampSim Data Plotting Utility")
    print("=" * 40)
    
    # Test with a single file
    try:
        baseline_data = read_csv_data("../data/baseline.csv")
        print(f"Loaded baseline data with {len(baseline_data)} workloads")
        print(f"Available workloads: {list_available_workloads(baseline_data)}")
        print(f"Available stats: {list_available_stats(baseline_data)}")
        
        # Show example data for first workload
        if baseline_data:
            first_workload = list(baseline_data.keys())[0]
            print(f"\nExample data for '{first_workload}':")
            for stat, value in baseline_data[first_workload].items():
                print(f"  {stat}: {value}")
                
        # Example: Get IPC values for all workloads
        ipc_data = get_stat_for_workloads(baseline_data, 'IPC')
        print(f"\nIPC values: {ipc_data}")
        
    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("Make sure to run this script from the scripts directory or adjust the path.")
