#!/usr/bin/env python3
"""
analyze_connectivity.py

Read a connectivity CSV (columns: id,time,node,online)
and print general stats:
 - number of time samples
 - number of nodes
 - overall fraction of “online” events
 - per-node online fraction (top/bottom performers)
 - time points with worst connectivity
"""
import argparse
import pandas as pd


def main():
    parser = argparse.ArgumentParser(
        description="Analyze connectivity.csv and print summary statistics."
    )
    parser.add_argument(
        "file",
        help="Path to connectivity CSV (id,time,node, online(0/1) or True/False)",
    )
    args = parser.parse_args()

    # Load
    df = pd.read_csv(args.file)
    # Identify online column
    status_cols = [c for c in df.columns if c not in ("id", "time", "node")]
    if len(status_cols) != 1:
        raise RuntimeError(f"Expected exactly one status column, found {status_cols}")
    df = df.rename(columns={status_cols[0]: "online"})

    # Normalize online to bool
    df["online"] = df["online"].astype(bool)

    # Basic counts
    times = df["time"].unique()
    nodes = df["node"].unique()
    total_samples = len(df)
    total_points = len(times) * len(nodes)

    print(f"Total rows         : {total_samples}")
    print(f"Unique time points : {len(times)}")
    print(f"Unique nodes       : {len(nodes)}")
    print(f"Expected (time×node): {total_points}")
    print()

    # Overall connectivity
    overall_online_frac = df["online"].mean()
    print(f"Overall online fraction: {overall_online_frac:.2%}")
    print()

    # Per-node
    per_node = df.groupby("node")["online"].mean().sort_values()
    print("Per-node online fraction (bottom 5 / top 5):")
    print(per_node.head(5).to_string(header=False))
    print("   …")
    print(per_node.tail(5).to_string(header=False))
    print()

    # Worst time slices
    per_time = df.groupby("time")["online"].mean().sort_values()
    print("Time points with lowest connectivity:")
    for t, frac in per_time.head(5).items():
        print(f"  time={t:.2f}s → online {frac:.2%}")


if __name__ == "__main__":
    main()
