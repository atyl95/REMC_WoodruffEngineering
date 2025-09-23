#!/usr/bin/env python3
"""
Focused timing analysis - just the range summary with key statistics.
"""

import pandas as pd
import numpy as np
import sys

def timing_summary(csv_file):
    """Get focused timing summary for 98-102μs range analysis."""
    
    print(f"Analyzing {csv_file}...")
    
    # Read the CSV file
    df = pd.read_csv(csv_file)
    sample_timestamp_us = df['sample_timestamp_us'].values
    
    # Calculate time differences between consecutive samples
    time_diffs = np.diff(sample_timestamp_us)
    
    # Expected interval
    expected_interval = 100.0
    min_acceptable = 98.0
    max_acceptable = 102.0
    
    # Basic statistics
    mean_interval = np.mean(time_diffs)
    median_interval = np.median(time_diffs)
    std_interval = np.std(time_diffs)
    min_interval = np.min(time_diffs)
    max_interval = np.max(time_diffs)
    
    # Range analysis
    within_range = np.sum((time_diffs >= min_acceptable) & (time_diffs <= max_acceptable))
    outside_range = np.sum((time_diffs < min_acceptable) | (time_diffs > max_acceptable))
    too_fast = np.sum(time_diffs < min_acceptable)
    too_slow = np.sum(time_diffs > max_acceptable)
    
    total_samples = len(time_diffs)
    
    print("\n" + "="*60)
    print("TIMING RANGE ANALYSIS SUMMARY")
    print("="*60)
    print(f"Total samples: {total_samples:,}")
    print(f"Expected interval: {expected_interval:.1f} μs")
    print(f"Acceptable range: {min_acceptable:.1f} - {max_acceptable:.1f} μs")
    print()
    print("STATISTICS:")
    print(f"  Mean:     {mean_interval:.3f} μs")
    print(f"  Median:   {median_interval:.3f} μs")
    print(f"  Std Dev:  {std_interval:.3f} μs")
    print(f"  Min:      {min_interval:.1f} μs")
    print(f"  Max:      {max_interval:.1f} μs")
    print(f"  Range:    {max_interval - min_interval:.1f} μs")
    print()
    print("RANGE ANALYSIS:")
    print(f"  Within 98-102μs:  {within_range:,} ({within_range/total_samples*100:.2f}%)")
    print(f"  Outside range:    {outside_range:,} ({outside_range/total_samples*100:.2f}%)")
    print(f"    Too fast (<98μs):  {too_fast:,} ({too_fast/total_samples*100:.2f}%)")
    print(f"    Too slow (>102μs): {too_slow:,} ({too_slow/total_samples*100:.2f}%)")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python timing_summary.py <csv_file_path>")
        print("Example: python timing_summary.py '../run_2.csv'")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    timing_summary(csv_file)
