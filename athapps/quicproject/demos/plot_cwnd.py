#!/usr/bin/env python3
"""Plot cwnd over time from QUIC simulation CSV output.

Usage: python plot_cwnd.py [csv_file...]

If multiple files are given, they're overlaid on the same plot
for side-by-side comparison.

CSV format expected (one line per ACK event):
    timestamp_ms, cwnd, bytes_in_flight, srtt_ms, total_lost
"""

import sys
import csv
import matplotlib.pyplot as plt


def load_csv(path):
    ts, cwnd = [], []
    with open(path) as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) < 2:
                continue
            try:
                ts.append(int(row[0]))
                cwnd.append(int(row[1]))
            except ValueError:
                continue  # skip header
    # Normalize timestamps to start at 0
    if ts:
        t0 = ts[0]
        ts = [t - t0 for t in ts]
    return ts, cwnd


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python plot_cwnd.py <csv_file> [csv_file2] ...")
        sys.exit(1)

    plt.figure(figsize=(12, 6))
    for path in sys.argv[1:]:
        ts, cwnd = load_csv(path)
        label = path.replace(".csv", "").split("/")[-1]
        plt.plot(ts, cwnd, label=label, linewidth=1.5)

    plt.xlabel("Time (ms)")
    plt.ylabel("Congestion Window (bytes)")
    plt.title("QUIC Congestion Window Over Time")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig("cwnd_plot.png", dpi=150)
    plt.show()
    print("Saved to cwnd_plot.png")
