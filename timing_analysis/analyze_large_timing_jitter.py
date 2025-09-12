#!/usr/bin/env python3
"""
Analyze timing jitter in large REMC telemetry data.
Optimized for large datasets with 300k+ samples.
Expected sample interval: ~100 microseconds
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import time
import sys

def analyze_large_timing_jitter(csv_file):
    """Analyze timing jitter in the sample_micros column for large datasets."""
    
    print(f"Reading large CSV file: {csv_file}")
    print("This may take a moment for 300k+ samples...")
    
    start_time = time.time()
    
    # Read the CSV file in chunks to be memory efficient
    chunk_size = 10000
    chunks = []
    
    for chunk in pd.read_csv(csv_file, chunksize=chunk_size):
        chunks.append(chunk)
        if len(chunks) % 10 == 0:
            print(f"  Read {len(chunks) * chunk_size:,} samples...")
    
    # Combine all chunks
    df = pd.concat(chunks, ignore_index=True)
    
    read_time = time.time() - start_time
    print(f"✓ File read complete in {read_time:.1f} seconds")
    print(f"Total samples: {len(df):,}")
    
    # Get the sample_micros column
    sample_micros = df['sample_micros'].values
    
    print("Calculating time differences...")
    # Calculate time differences between consecutive samples
    time_diffs = np.diff(sample_micros)
    
    # Expected interval is 100 microseconds
    expected_interval = 100.0
    
    print("Computing statistics...")
    # Calculate statistics
    mean_interval = np.mean(time_diffs)
    std_interval = np.std(time_diffs)
    min_interval = np.min(time_diffs)
    max_interval = np.max(time_diffs)
    
    # Find samples with large deviations from expected interval
    # Define "large jitter" as more than 2 standard deviations from mean
    # or more than 10% deviation from expected interval
    jitter_threshold_std = 2 * std_interval
    jitter_threshold_pct = 0.1 * expected_interval  # 10% of 100us = 10us
    
    print("Identifying jitters...")
    large_jitters_std = np.abs(time_diffs - mean_interval) > jitter_threshold_std
    large_jitters_pct = np.abs(time_diffs - expected_interval) > jitter_threshold_pct
    
    # Find samples with very large jitters (>50% deviation)
    very_large_jitters = np.abs(time_diffs - expected_interval) > 0.5 * expected_interval
    
    # Find gaps (>2x expected interval)
    gap_threshold = 2 * expected_interval  # More than 200us between samples
    gaps = time_diffs > gap_threshold
    
    print("\n" + "="*80)
    print("LARGE DATASET TIMING ANALYSIS RESULTS")
    print("="*80)
    print(f"Total samples: {len(sample_micros):,}")
    print(f"Time differences calculated: {len(time_diffs):,}")
    print(f"Expected interval: {expected_interval:.1f} μs")
    print(f"Actual mean interval: {mean_interval:.3f} μs")
    print(f"Standard deviation: {std_interval:.3f} μs")
    print(f"Min interval: {min_interval:.3f} μs")
    print(f"Max interval: {max_interval:.3f} μs")
    print(f"Mean deviation from expected: {abs(mean_interval - expected_interval):.3f} μs")
    
    print(f"\nJITTER ANALYSIS:")
    print(f"Samples with >2σ deviation from mean: {np.sum(large_jitters_std):,}")
    print(f"Samples with >10% deviation from expected: {np.sum(large_jitters_pct):,}")
    print(f"Samples with >50% deviation from expected: {np.sum(very_large_jitters):,}")
    print(f"Potential gaps (>200μs): {np.sum(gaps):,}")
    
    if np.sum(very_large_jitters) > 0:
        print(f"\n⚠️  LARGE JITTERS DETECTED:")
        jitter_indices = np.where(very_large_jitters)[0]
        for i, idx in enumerate(jitter_indices[:20]):  # Show first 20
            sample_num = idx + 1
            actual_interval = time_diffs[idx]
            deviation = actual_interval - expected_interval
            deviation_pct = (deviation / expected_interval) * 100
            print(f"  Sample {sample_num:,}: {actual_interval:.1f} μs (deviation: {deviation:+.1f} μs, {deviation_pct:+.1f}%)")
        
        if len(jitter_indices) > 20:
            print(f"  ... and {len(jitter_indices) - 20:,} more large jitters")
    
    # Check for gaps
    if np.sum(gaps) > 0:
        print(f"\n⚠️  GAPS DETECTED:")
        gap_indices = np.where(gaps)[0]
        print(f"Total gaps: {len(gap_indices):,}")
        
        # Show largest gaps
        gap_values = time_diffs[gaps]
        gap_indices_with_values = list(zip(gap_indices, gap_values))
        gap_indices_with_values.sort(key=lambda x: x[1], reverse=True)
        
        print("Largest gaps:")
        for i, (idx, gap_value) in enumerate(gap_indices_with_values[:10]):
            sample_num = idx + 1
            gap_ms = gap_value / 1000
            print(f"  Sample {sample_num:,}: {gap_value:.1f} μs ({gap_ms:.1f} ms)")
    
    # Create a simplified plot for large datasets
    print("\nGenerating visualization...")
    plt.figure(figsize=(15, 10))
    
    # Sample every 1000th point for plotting to avoid memory issues
    plot_step = max(1, len(time_diffs) // 10000)  # Show max 10k points
    plot_indices = np.arange(0, len(time_diffs), plot_step)
    plot_diffs = time_diffs[plot_indices]
    
    # Plot 1: Time differences over sample number (sampled)
    plt.subplot(2, 1, 1)
    plt.plot(plot_indices, plot_diffs, 'b-', alpha=0.7, linewidth=0.5)
    plt.axhline(y=expected_interval, color='r', linestyle='--', label=f'Expected: {expected_interval} μs')
    plt.axhline(y=mean_interval, color='g', linestyle='-', label=f'Mean: {mean_interval:.1f} μs')
    plt.axhline(y=mean_interval + 2*std_interval, color='orange', linestyle=':', alpha=0.7, label='±2σ')
    plt.axhline(y=mean_interval - 2*std_interval, color='orange', linestyle=':', alpha=0.7)
    plt.xlabel('Sample Number')
    plt.ylabel('Time Difference (μs)')
    plt.title(f'Sample Intervals Over Time (showing every {plot_step}th sample)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Plot 2: Histogram of time differences (sampled)
    plt.subplot(2, 1, 2)
    plt.hist(plot_diffs, bins=100, alpha=0.7, edgecolor='black')
    plt.axvline(x=expected_interval, color='r', linestyle='--', label=f'Expected: {expected_interval} μs')
    plt.axvline(x=mean_interval, color='g', linestyle='-', label=f'Mean: {mean_interval:.1f} μs')
    plt.xlabel('Time Difference (μs)')
    plt.ylabel('Frequency')
    plt.title(f'Distribution of Sample Intervals (showing every {plot_step}th sample)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('large_timing_analysis.png', dpi=150, bbox_inches='tight')
    print(f"📊 Plot saved as 'large_timing_analysis.png'")
    
    # Summary
    print(f"\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    
    # Calculate percentage of samples with issues
    total_samples = len(time_diffs)
    jitter_percentage = (np.sum(large_jitters_pct) / total_samples) * 100
    gap_percentage = (np.sum(gaps) / total_samples) * 100
    
    if np.sum(very_large_jitters) == 0 and np.sum(gaps) == 0:
        print("✅ No significant timing jitters detected!")
        print(f"   Timing is consistent with expected {expected_interval}μs intervals")
    else:
        print("⚠️  Timing issues detected:")
        if np.sum(very_large_jitters) > 0:
            print(f"   - {np.sum(very_large_jitters):,} samples with >50% timing deviation ({jitter_percentage:.2f}%)")
        if np.sum(gaps) > 0:
            print(f"   - {np.sum(gaps):,} potential gaps in sampling ({gap_percentage:.2f}%)")
    
    print(f"\nPerformance metrics:")
    print(f"   - Data processing time: {time.time() - start_time:.1f} seconds")
    print(f"   - Average interval accuracy: {abs(mean_interval - expected_interval):.3f} μs deviation")
    print(f"   - Timing consistency: {std_interval:.3f} μs standard deviation")
    
    return {
        'mean_interval': mean_interval,
        'std_interval': std_interval,
        'large_jitters': np.sum(large_jitters_pct),
        'very_large_jitters': np.sum(very_large_jitters),
        'gaps': np.sum(gaps),
        'total_samples': total_samples
    }

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python analyze_large_timing_jitter.py <csv_file_path>")
        print("Example: python analyze_large_timing_jitter.py '../csv dumps/run_2.csv'")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    results = analyze_large_timing_jitter(csv_file)
