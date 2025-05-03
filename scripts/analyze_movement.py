#!/usr/bin/env python3
"""
analyze_movement.py

Read a movement CSV (columns: id,time,node,x,y,z,speed)
and print:
 - number of time samples, nodes
 - simulation duration
 - bounding box of x,y
 - global & per-node speed statistics
 - total distance traveled per node (top movers)
"""
import argparse
import pandas as pd
import numpy as np


def compute_total_distance(sub: pd.DataFrame) -> float:
    # assumes sub sorted by time
    dx = sub["x"].diff().fillna(0.0)
    dy = sub["y"].diff().fillna(0.0)
    dz = sub["z"].diff().fillna(0.0)
    return np.sqrt(dx * dx + dy * dy + dz * dz).sum()


def main():
    parser = argparse.ArgumentParser(
        description="Analyze movement.csv and print summary statistics."
    )
    parser.add_argument("file", help="Path to movement CSV (id,time,node,x,y,z,speed)")
    args = parser.parse_args()

    # Load
    df = pd.read_csv(args.file)

    # Counts & duration
    times = df["time"].unique()
    nodes = df["node"].unique()
    tmin, tmax = df["time"].min(), df["time"].max()
    print(f"Unique time points : {len(times)}")
    print(f"Unique nodes       : {len(nodes)}")
    print(
        f"Simulation runs from {tmin:.2f}s to {tmax:.2f}s (duration {tmax-tmin:.2f}s)"
    )
    print()

    # Bounding box
    xmin, xmax = df["x"].min(), df["x"].max()
    ymin, ymax = df["y"].min(), df["y"].max()
    print(f"X range: {xmin:.2f} → {xmax:.2f}")
    print(f"Y range: {ymin:.2f} → {ymax:.2f}")
    print()

    # Speed stats
    print("Global speed stats (m/s):")
    print(f"  mean: {df['speed'].mean():.3f}")
    print(f"  std : {df['speed'].std():.3f}")
    print(f"  min : {df['speed'].min():.3f}")
    print(f"  max : {df['speed'].max():.3f}")
    print()

    # Per-node speed
    per_node_speed = df.groupby("node")["speed"]
    speed_mean = per_node_speed.mean().sort_values()
    print("Per-node mean speed (bottom 5 / top 5):")
    print(speed_mean.head(5).to_string(header=False))
    print("   …")
    print(speed_mean.tail(5).to_string(header=False))
    print()

    # Total distance traveled
    print("Computing total distance traveled per node…")
    dists = {}
    for node, sub in df.groupby("node"):
        sub_sorted = sub.sort_values("time")
        dists[node] = compute_total_distance(sub_sorted)
    dist_ser = pd.Series(dists).sort_values()
    print("Total distance per node (bottom 5 / top 5):")
    print(dist_ser.head(5).to_string(header=False))
    print("   …")
    print(dist_ser.tail(5).to_string(header=False))


if __name__ == "__main__":
    main()
