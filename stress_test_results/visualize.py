#!/usr/bin/env python3
"""
Soft-UE 200Gbps Stress Test Visualization Script
Generates performance charts from CSV data
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Set style
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['figure.figsize'] = (12, 8)
plt.rcParams['font.size'] = 12

def load_data(results_dir='.'):
    """Load CSV data files"""
    summary = pd.read_csv(os.path.join(results_dir, 'summary.csv'))
    latency = pd.read_csv(os.path.join(results_dir, 'latency_distribution.csv'))
    percentiles = pd.read_csv(os.path.join(results_dir, 'latency_percentiles.csv'))

    # Load throughput time series if available
    timeseries_path = os.path.join(results_dir, 'throughput_timeseries.csv')
    if os.path.exists(timeseries_path):
        timeseries = pd.read_csv(timeseries_path)
    else:
        timeseries = None

    return summary, latency, percentiles, timeseries

def create_summary_chart(summary, output_dir='.'):
    """Create summary metrics bar chart"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # Extract metrics
    metrics = dict(zip(summary['Metric'], summary['Value']))

    # 1. Throughput comparison
    ax1 = axes[0, 0]
    throughput_data = [float(metrics['Target Rate']), float(metrics['Achieved Throughput'])]
    bars = ax1.bar(['Target', 'Achieved'], throughput_data, color=['#3498db', '#2ecc71'])
    ax1.set_ylabel('Throughput (Gbps)')
    ax1.set_title('Throughput Comparison')
    ax1.set_ylim(0, max(throughput_data) * 1.2)
    for bar, val in zip(bars, throughput_data):
        ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 2,
                f'{val:.1f}', ha='center', fontsize=14, fontweight='bold')

    # 2. Efficiency gauge
    ax2 = axes[0, 1]
    efficiency = min(float(metrics['Efficiency']), 100)  # Cap at 100% for display
    actual_efficiency = float(metrics['Efficiency'])
    colors = ['#2ecc71' if actual_efficiency >= 90 else '#f39c12' if actual_efficiency >= 70 else '#e74c3c']
    ax2.pie([efficiency, max(0, 100-efficiency)], labels=['Efficiency', ''],
            colors=[colors[0], '#ecf0f1'], startangle=90,
            autopct=lambda p: f'{actual_efficiency:.1f}%' if p > 50 else '')
    ax2.set_title('Throughput Efficiency')

    # 3. Packet statistics
    ax3 = axes[1, 0]
    packet_data = [int(float(metrics['Total Packets Sent'])),
                   int(float(metrics['Total Packets Received']))]
    bars = ax3.bar(['Sent', 'Received'], packet_data, color=['#9b59b6', '#1abc9c'])
    ax3.set_ylabel('Packets')
    ax3.set_title('Packet Statistics')
    for bar, val in zip(bars, packet_data):
        ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 50,
                f'{val:,}', ha='center', fontsize=12)

    # 4. Latency metrics
    ax4 = axes[1, 1]
    latency_metrics = ['Average Latency', 'P50 Latency', 'P99 Latency', 'Min Latency', 'Max Latency']
    latency_values = [float(metrics[m]) for m in latency_metrics]
    labels = ['Avg', 'P50', 'P99', 'Min', 'Max']
    bars = ax4.bar(labels, latency_values, color='#e74c3c')
    ax4.set_ylabel('Latency (ns)')
    ax4.set_title('Latency Analysis')
    for bar, val in zip(bars, latency_values):
        ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                f'{val:.1f}', ha='center', fontsize=10)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'summary_chart.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: summary_chart.png")

def create_latency_histogram(latency, output_dir='.'):
    """Create latency distribution histogram"""
    fig, ax = plt.subplots(figsize=(12, 6))

    data = latency['Latency_ns'].values

    # Create histogram
    n, bins, patches = ax.hist(data, bins=50, color='#3498db', edgecolor='white', alpha=0.7)

    # Add statistics lines
    mean_val = np.mean(data)
    p50 = np.percentile(data, 50)
    p99 = np.percentile(data, 99)

    ax.axvline(mean_val, color='#e74c3c', linestyle='--', linewidth=2, label=f'Mean: {mean_val:.1f} ns')
    ax.axvline(p50, color='#2ecc71', linestyle='-.', linewidth=2, label=f'P50: {p50:.1f} ns')
    ax.axvline(p99, color='#9b59b6', linestyle=':', linewidth=2, label=f'P99: {p99:.1f} ns')

    ax.set_xlabel('Latency (ns)')
    ax.set_ylabel('Frequency')
    ax.set_title('Latency Distribution')
    ax.legend()

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'latency_histogram.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: latency_histogram.png")

def create_percentile_chart(percentiles, output_dir='.'):
    """Create latency percentile chart"""
    fig, ax = plt.subplots(figsize=(12, 6))

    ax.plot(percentiles['Percentile'], percentiles['Latency_ns'],
            'o-', color='#3498db', linewidth=2, markersize=6)

    ax.fill_between(percentiles['Percentile'], 0, percentiles['Latency_ns'],
                    alpha=0.3, color='#3498db')

    ax.set_xlabel('Percentile')
    ax.set_ylabel('Latency (ns)')
    ax.set_title('Latency Percentile Distribution')
    ax.set_xlim(0, 100)
    ax.set_xticks(range(0, 101, 10))

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'latency_percentiles.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: latency_percentiles.png")

def create_performance_report(summary, output_dir='.'):
    """Create text performance report"""
    metrics = dict(zip(summary['Metric'], summary['Value']))

    report = f"""
================================================================================
                    SOFT-UE 200Gbps STRESS TEST REPORT
================================================================================

TEST CONFIGURATION
------------------
  Target Rate:        {metrics['Target Rate']} Gbps
  Packet Count:       {int(float(metrics['Total Packets Sent'])):,} packets

THROUGHPUT RESULTS
------------------
  Achieved:           {float(metrics['Achieved Throughput']):.3f} Gbps
  Efficiency:         {float(metrics['Efficiency']):.2f}%
  Packet Rate:        {int(float(metrics['Packet Rate'])):,} pps

PACKET STATISTICS
-----------------
  Sent:               {int(float(metrics['Total Packets Sent'])):,}
  Received:           {int(float(metrics['Total Packets Received'])):,}
  Loss Rate:          {float(metrics['Packet Loss Rate']):.4f}%
  Bytes Transferred:  {int(float(metrics['Total Bytes Received'])):,} bytes

LATENCY ANALYSIS
----------------
  Average:            {float(metrics['Average Latency']):.2f} ns
  P50:                {float(metrics['P50 Latency']):.2f} ns
  P99:                {float(metrics['P99 Latency']):.2f} ns
  Min:                {float(metrics['Min Latency']):.2f} ns
  Max:                {float(metrics['Max Latency']):.2f} ns
  Jitter:             {float(metrics['Jitter']):.2f} ns

PROTOCOL STATISTICS
-------------------
  SES Processed:      {int(float(metrics['SES Processed'])):,}
  PDS Processed:      {int(float(metrics['PDS Processed'])):,}

ASSESSMENT
----------
"""
    efficiency = float(metrics['Efficiency'])
    if efficiency >= 90:
        report += "  Status: EXCELLENT - Ultra Ethernet protocol stack performing at optimal level\n"
    elif efficiency >= 70:
        report += "  Status: GOOD - Protocol stack performing well with room for optimization\n"
    elif efficiency >= 50:
        report += "  Status: MODERATE - Consider optimizing PDC allocation and batching\n"
    else:
        report += "  Status: NEEDS IMPROVEMENT - Review packet processing pipeline\n"

    report += "=" * 80 + "\n"

    with open(os.path.join(output_dir, 'performance_report.txt'), 'w') as f:
        f.write(report)
    print(f"Saved: performance_report.txt")
    print(report)

def create_throughput_timeseries(timeseries, output_dir='.'):
    """Create throughput over time chart"""
    if timeseries is None or timeseries.empty:
        print("No time series data available")
        return

    fig, ax = plt.subplots(figsize=(14, 6))

    # Convert time to microseconds for readability
    time_us = timeseries['Time_us'].values
    throughput = timeseries['Throughput_Gbps'].values

    # Plot throughput over time
    ax.plot(time_us, throughput, 'b-', linewidth=1.5, alpha=0.8, label='Throughput')

    # Add moving average
    window = min(10, len(throughput) // 5) if len(throughput) > 10 else 1
    if window > 1:
        moving_avg = pd.Series(throughput).rolling(window=window, center=True).mean()
        ax.plot(time_us, moving_avg, 'r-', linewidth=2, label=f'Moving Avg (n={window})')

    # Add target line
    ax.axhline(y=200, color='#27ae60', linestyle='--', linewidth=2, label='Target: 200 Gbps')

    # Calculate and show average
    avg_throughput = np.mean(throughput)
    ax.axhline(y=avg_throughput, color='#e74c3c', linestyle=':', linewidth=2,
               label=f'Average: {avg_throughput:.1f} Gbps')

    ax.set_xlabel('Time (μs)')
    ax.set_ylabel('Throughput (Gbps)')
    ax.set_title('Throughput Over Time - 200Gbps Stress Test')
    ax.legend(loc='lower right')
    ax.set_ylim(0, max(throughput) * 1.2)

    # Add grid
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'throughput_timeseries.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: throughput_timeseries.png")

def main():
    results_dir = '.'

    print("Loading data...")
    summary, latency, percentiles, timeseries = load_data(results_dir)

    print("\nGenerating visualizations...")
    create_summary_chart(summary, results_dir)
    create_latency_histogram(latency, results_dir)
    create_percentile_chart(percentiles, results_dir)
    create_throughput_timeseries(timeseries, results_dir)
    create_performance_report(summary, results_dir)

    print("\nVisualization complete!")

if __name__ == '__main__':
    main()
