# REMC Timing Analysis Tools

This folder contains Python scripts for analyzing the timing performance of the telemetry system's 100μs sampling intervals.

## Scripts

### `timing_summary.py` ⭐ **MAIN SCRIPT**
**Purpose:** Quick focused analysis of timing performance  
**Usage:** `python timing_summary.py <csv_file_path>`  
**Example:** `python timing_summary.py "../csv dumps/run_2.csv"`  
**Output:** 
- Basic statistics (mean, median, std dev, min/max, range)
- Count of samples within/outside the 98-102μs acceptable range
- Percentage breakdown of timing performance

**This is the primary script we use for evaluating timing improvements.**

### `analyze_large_timing_jitter.py`
**Purpose:** Comprehensive analysis with visualizations for large datasets (300k+ samples)  
**Usage:** `python analyze_large_timing_jitter.py <csv_file_path>`  
**Example:** `python analyze_large_timing_jitter.py "../csv dumps/run_2.csv"`  
**Output:**
- Detailed timing statistics
- Plots showing timing intervals over time
- Histogram of sample interval distribution
- Detection of gaps and large jitters

### `detailed_timing_analysis.py`
**Purpose:** Focused analysis around specific timing problems  
**Usage:** `python detailed_timing_analysis.py <csv_file_path>`  
**Example:** `python detailed_timing_analysis.py "../csv dumps/run_0.csv"`  
**Output:**
- Detailed view of samples around large timing gaps
- Analysis of specific problematic areas
- Gap duration calculations

## Key Metrics
- **Target interval:** 100μs
- **Acceptable range:** 98-102μs  
- **Excellent performance:** >95% within range
- **Good performance:** >90% within range

## Performance Results Summary
| Run | Within 98-102μs | Outside Range | Std Dev | Notes |
|-----|-----------------|---------------|---------|-------|
| 0   | 85.34%         | 14.66%        | 3.641μs | Original |
| 1   | 85.26%         | 14.74%        | 3.637μs | Similar |
| 2   | 97.05%         | 2.95%         | 1.662μs | ⭐ Optimized |

## Dependencies
```bash
pip install pandas numpy matplotlib
```

## Quick Start
```bash
cd timing_analysis

# Analyze a specific CSV file
python timing_summary.py "../csv dumps/run_2.csv"

# Comprehensive analysis with plots
python analyze_large_timing_jitter.py "../csv dumps/run_2.csv"

# Detailed gap analysis
python detailed_timing_analysis.py "../csv dumps/run_0.csv"
```

## Command Line Usage
All scripts now accept the CSV file path as a command line argument:
- **Required:** `<csv_file_path>` - Path to the CSV file to analyze
- **Relative paths:** Use `"../csv dumps/run_X.csv"` from the timing_analysis folder
- **Absolute paths:** Full file paths are also supported
