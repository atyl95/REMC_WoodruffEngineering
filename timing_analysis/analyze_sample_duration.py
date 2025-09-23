#!/usr/bin/env python3
"""
Analyze sample duration (time between start and end timestamps) in REMC telemetry data.
This script measures the duration of each sample by calculating the difference between
sample_timestamp_us_end and sample_timestamp_us for each sample.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import time
import sys

def analyze_sample_duration(csv_file):
    """Analyze the duration of each sample based on start and end timestamps."""
    
    print(f"Reading CSV file: {csv_file}")
    print("Analyzing sample durations...")
    
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
    
    # Check if we have the required columns
    if 'sample_timestamp_us' not in df.columns or 'sample_timestamp_us_end' not in df.columns:
        print("❌ Error: Required timestamp columns not found in CSV")
        print("Available columns:", list(df.columns))
        return None
    
    # Get the timestamp columns
    start_timestamps = df['sample_timestamp_us'].values
    end_timestamps = df['sample_timestamp_us_end'].values
    
    # Calculate sample durations (end - start)
    print("Calculating sample durations...")
    sample_durations = end_timestamps - start_timestamps
    
    # Remove any invalid durations (negative or NaN values)
    valid_mask = (sample_durations >= 0) & ~np.isnan(sample_durations)
    valid_durations = sample_durations[valid_mask]
    invalid_count = np.sum(~valid_mask)
    
    if invalid_count > 0:
        print(f"⚠️  Found {invalid_count} invalid durations (negative or NaN)")
    
    if len(valid_durations) == 0:
        print("❌ No valid sample durations found!")
        return None
    
    print(f"Valid sample durations: {len(valid_durations):,}")
    
    # Calculate statistics
    print("Computing statistics...")
    mean_duration = np.mean(valid_durations)
    std_duration = np.std(valid_durations)
    min_duration = np.min(valid_durations)
    max_duration = np.max(valid_durations)
    median_duration = np.median(valid_durations)
    
    # Find samples with unusual durations
    # Define thresholds for analysis
    duration_threshold_1us = 1.0  # 1 microsecond
    duration_threshold_10us = 10.0  # 10 microseconds
    duration_threshold_100us = 100.0  # 100 microseconds
    
    samples_1us = np.sum(valid_durations >= duration_threshold_1us)
    samples_10us = np.sum(valid_durations >= duration_threshold_10us)
    samples_100us = np.sum(valid_durations >= duration_threshold_100us)
    
    # Find samples with very long durations (potential outliers)
    outlier_threshold = mean_duration + 3 * std_duration
    outliers = valid_durations > outlier_threshold
    
    print("\n" + "="*80)
    print("SAMPLE DURATION ANALYSIS RESULTS")
    print("="*80)
    print(f"Total samples analyzed: {len(valid_durations):,}")
    print(f"Invalid durations: {invalid_count:,}")
    print(f"Mean duration: {mean_duration:.3f} μs")
    print(f"Median duration: {median_duration:.3f} μs")
    print(f"Standard deviation: {std_duration:.3f} μs")
    print(f"Min duration: {min_duration:.3f} μs")
    print(f"Max duration: {max_duration:.3f} μs")
    print(f"3σ upper bound: {mean_duration + 3*std_duration:.3f} μs")
    
    print(f"\nDURATION DISTRIBUTION:")
    print(f"Samples ≥ 1μs: {samples_1us:,} ({samples_1us/len(valid_durations)*100:.1f}%)")
    print(f"Samples ≥ 10μs: {samples_10us:,} ({samples_10us/len(valid_durations)*100:.1f}%)")
    print(f"Samples ≥ 100μs: {samples_100us:,} ({samples_100us/len(valid_durations)*100:.1f}%)")
    print(f"Outliers (>3σ): {np.sum(outliers):,} ({np.sum(outliers)/len(valid_durations)*100:.1f}%)")
    
    # Show outliers if any
    if np.sum(outliers) > 0:
        print(f"\n⚠️  OUTLIERS DETECTED:")
        outlier_indices = np.where(outliers)[0]
        outlier_values = valid_durations[outliers]
        
        # Show first 20 outliers
        for i, (idx, duration) in enumerate(zip(outlier_indices[:20], outlier_values[:20])):
            sample_num = idx + 1
            duration_ms = duration / 1000
            print(f"  Sample {sample_num:,}: {duration:.1f} μs ({duration_ms:.3f} ms)")
        
        if len(outlier_indices) > 20:
            print(f"  ... and {len(outlier_indices) - 20:,} more outliers")
    
    # Check for zero durations (start == end)
    zero_durations = np.sum(valid_durations == 0)
    if zero_durations > 0:
        print(f"\n📝 ZERO DURATIONS:")
        print(f"Samples with zero duration (start == end): {zero_durations:,} ({zero_durations/len(valid_durations)*100:.1f}%)")
        print("This may indicate that the end timestamp is not being properly set.")
    
    # Create comprehensive visualization
    print("\nGenerating visualization...")
    plt.figure(figsize=(15, 8))
    
    # Plot 1: Sample durations over time (scatter plot)
    plt.subplot(2, 1, 1)
    plt.scatter(range(len(valid_durations)), valid_durations, s=1, alpha=0.6, c='blue')
    plt.axhline(y=mean_duration, color='r', linestyle='-', linewidth=2, label=f'Mean: {mean_duration:.1f} μs')
    plt.axhline(y=mean_duration + 2*std_duration, color='orange', linestyle=':', linewidth=2, label=f'+2σ: {mean_duration + 2*std_duration:.1f} μs')
    plt.axhline(y=mean_duration - 2*std_duration, color='orange', linestyle=':', linewidth=2, label=f'-2σ: {mean_duration - 2*std_duration:.1f} μs')
    plt.xlabel('Sample Number')
    plt.ylabel('Sample Duration (μs)')
    plt.title(f'Sample Durations Over Time ({len(valid_durations):,} samples)\nσ = {std_duration:.3f} μs')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Plot 2: Histogram of sample durations
    plt.subplot(2, 1, 2)
    plt.hist(valid_durations, bins=100, alpha=0.7, edgecolor='black')
    plt.axvline(x=mean_duration, color='r', linestyle='-', linewidth=2, label=f'Mean: {mean_duration:.1f} μs')
    plt.axvline(x=median_duration, color='g', linestyle='--', linewidth=2, label=f'Median: {median_duration:.1f} μs')
    plt.axvline(x=mean_duration + 2*std_duration, color='orange', linestyle=':', alpha=0.7, label=f'+2σ: {mean_duration + 2*std_duration:.1f} μs')
    plt.axvline(x=mean_duration - 2*std_duration, color='orange', linestyle=':', alpha=0.7, label=f'-2σ: {mean_duration - 2*std_duration:.1f} μs')
    plt.xlabel('Sample Duration (μs)')
    plt.ylabel('Frequency')
    plt.title(f'Distribution of Sample Durations ({len(valid_durations):,} samples)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('sample_duration_analysis.png', dpi=150, bbox_inches='tight')
    print(f"📊 Plot saved as 'sample_duration_analysis.png'")
    
    # Summary
    print(f"\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    
    # Calculate percentage of samples with different duration characteristics
    total_valid = len(valid_durations)
    zero_pct = (zero_durations / total_valid) * 100
    outlier_pct = (np.sum(outliers) / total_valid) * 100
    
    if zero_durations == total_valid:
        print("⚠️  All sample durations are zero!")
        print("   This indicates that start and end timestamps are identical.")
        print("   The Arduino code may not be properly setting the end timestamp.")
    elif zero_durations > total_valid * 0.5:
        print("⚠️  Most sample durations are zero!")
        print(f"   {zero_pct:.1f}% of samples have zero duration.")
        print("   This suggests the end timestamp is not being properly set.")
    elif mean_duration < 1.0:
        print("✅ Sample durations are very short (< 1μs)")
        print("   This indicates very fast sample processing.")
    elif mean_duration < 10.0:
        print("✅ Sample durations are short (< 10μs)")
        print("   This indicates fast sample processing.")
    else:
        print("📊 Sample durations show normal variation")
        print(f"   Mean duration: {mean_duration:.1f} μs")
    
    if np.sum(outliers) > 0:
        print(f"⚠️  {np.sum(outliers):,} outliers detected ({outlier_pct:.1f}%)")
    
    print(f"\nPerformance metrics:")
    print(f"   - Data processing time: {time.time() - start_time:.1f} seconds")
    print(f"   - Duration consistency: {std_duration:.3f} μs standard deviation")
    print(f"   - Duration range: {min_duration:.3f} - {max_duration:.3f} μs")
    
    return {
        'mean_duration': mean_duration,
        'std_duration': std_duration,
        'zero_durations': zero_durations,
        'outliers': np.sum(outliers),
        'total_samples': total_valid,
        'invalid_durations': invalid_count
    }

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python analyze_sample_duration.py <csv_file_path>")
        print("Example: python analyze_sample_duration.py '../remc_telemetry_log_20250923_141849.csv'")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    results = analyze_sample_duration(csv_file)
    
    if results is None:
        sys.exit(1)
