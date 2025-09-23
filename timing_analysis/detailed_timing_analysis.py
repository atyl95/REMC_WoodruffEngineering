#!/usr/bin/env python3
"""
Detailed timing analysis focusing on the problematic area.
"""

import csv
import sys

def analyze_timing_detailed(csv_file):
    """Analyze timing around the problematic samples."""
    
    print("Detailed Timing Analysis")
    print("=" * 50)
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        samples = list(reader)
    
    print(f"Total samples: {len(samples)}")
    print()
    
    # Convert sample_timestamp_us to integers
    sample_timestamp_us = [int(row['sample_timestamp_us']) for row in samples]
    
    # Calculate time differences
    time_diffs = []
    for i in range(1, len(sample_timestamp_us)):
        diff = sample_timestamp_us[i] - sample_timestamp_us[i-1]
        time_diffs.append(diff)
    
    # Find the large gap
    max_diff = max(time_diffs)
    max_diff_index = time_diffs.index(max_diff)
    
    print(f"Largest time difference: {max_diff} μs")
    print(f"Occurs between samples {max_diff_index} and {max_diff_index + 1}")
    print()
    
    # Show samples around the gap
    start_idx = max(0, max_diff_index - 5)
    end_idx = min(len(samples), max_diff_index + 6)
    
    print("Samples around the large gap:")
    print("-" * 80)
    print(f"{'Sample':<8} {'Time (μs)':<12} {'Diff (μs)':<12} {'Timestamp':<20}")
    print("-" * 80)
    
    for i in range(start_idx, end_idx):
        sample_num = i + 1
        time_us = sample_timestamp_us[i]
        diff = time_diffs[i-1] if i > 0 else 0
        timestamp = samples[i]['timestamp']
        
        marker = " <-- GAP" if i == max_diff_index else ""
        print(f"{sample_num:<8} {time_us:<12} {diff:<12} {timestamp:<20}{marker}")
    
    print()
    
    # Analyze the gap more
    gap_start_time = sample_timestamp_us[max_diff_index]
    gap_end_time = sample_timestamp_us[max_diff_index + 1]
    gap_duration = gap_end_time - gap_start_time
    
    print(f"Gap analysis:")
    print(f"  Start time: {gap_start_time} μs")
    print(f"  End time: {gap_end_time} μs")
    print(f"  Duration: {gap_duration} μs")
    print(f"  Duration: {gap_duration/1000:.1f} ms")
    print(f"  Expected duration: {100} μs")
    print(f"  Excess duration: {gap_duration - 100} μs")
    
    # Check if there are other significant gaps
    print()
    print("Other significant gaps (>1000 μs):")
    print("-" * 50)
    
    significant_gaps = [(i, diff) for i, diff in enumerate(time_diffs) if diff > 1000]
    
    if significant_gaps:
        for sample_idx, diff in significant_gaps:
            print(f"  Sample {sample_idx + 1}: {diff} μs ({diff/1000:.1f} ms)")
    else:
        print("  None found")
    
    # Overall statistics
    print()
    print("Overall timing statistics:")
    print("-" * 30)
    
    # Filter out the large gap for normal statistics
    normal_diffs = [d for d in time_diffs if d < 1000]
    
    if normal_diffs:
        avg_normal = sum(normal_diffs) / len(normal_diffs)
        print(f"Normal intervals (excluding gaps): {len(normal_diffs)} samples")
        print(f"Average normal interval: {avg_normal:.1f} μs")
        print(f"Expected interval: 100 μs")
        print(f"Deviation from expected: {abs(avg_normal - 100):.1f} μs")
    
    print(f"Total gaps >1000μs: {len(significant_gaps)}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python detailed_timing_analysis.py <csv_file_path>")
        print("Example: python detailed_timing_analysis.py '../csv dumps/run_0.csv'")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    analyze_timing_detailed(csv_file)
