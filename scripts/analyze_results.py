#!/usr/bin/env python3
"""
analyze_results.py

One-stop analysis for MANET simulation outputs:
  - Packet QoS metrics (PDR, delay, throughput)
  - Mobility statistics (speed, distance, bounding box)
  - Connectivity summary (online fraction)
  - Optional movement plot with first-offline markers (×), and ability to disable markers

Usage:
  python3 analyze_results.py \
    --packets packets.csv --nodes 10 \
    --movement movement.csv \
    --connectivity connectivity.csv \
    [--plot output.png --xmax 50 --ymax 50] \
    [--no-mark-offline]
"""
import argparse
import os
import math
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patheffects as path_effects
from collections import Counter

def load_and_merge_packets(path: str):
    """
    Load packets CSV, split sends/receives, merge them,
    and identify spine vs normal nodes. Also return time bounds.
    """
    df = pd.read_csv(path, dtype={"node": str})
    for col in ("node", "time", "uid", "received"):
        if col not in df.columns:
            raise ValueError(f"Missing column {col} in packets CSV")

    # split send vs receive
    df_send = df[df["received"] == 0].rename(columns={"time": "time_send"})
    df_recv = (
        df[df["received"] == 1]
        .loc[:, ["uid", "node"]]
        .rename(columns={"node": "recv_node"})
    )

    # merge so each send knows its (optional) receiver
    merged = pd.merge(
        df_send[["uid", "time_send", "node"]],
        df_recv,
        on="uid", how="left"
    )

    # identify spines vs normals
    spine_ids = sorted([n for n in merged["node"].unique() if n.endswith("S")])
    normal_ids = sorted([n for n in merged["node"].unique() if not n.endswith("S")])

    # time span
    t0 = df_send["time_send"].min()
    t1 = df_send["time_send"].max()

    return df_send, merged, normal_ids, spine_ids, t0, t1

def infer_series_size_from_runs(df_send: pd.DataFrame) -> int:
    """
    Sorts sends by time/uid, measures consecutive runs of the SAME node,
    and returns the most common run-length.
    """
    sends = df_send.sort_values(["time_send", "uid"], ignore_index=True)
    nodes = sends["node"].tolist()
    if not nodes:
        raise RuntimeError("No sends to infer series size from")

    run_lengths = []
    current = nodes[0]
    count = 1
    for n in nodes[1:]:
        if n == current:
            count += 1
        else:
            run_lengths.append(count)
            current = n
            count = 1
    run_lengths.append(count)

    return Counter(run_lengths).most_common(1)[0][0]

def compute_health(merged: pd.DataFrame, normal_ids, spine_ids, series_size: int):
    """
    Returns a DataFrame indexed by node with columns:
      total_series, healthy_series, health_fraction
    """
    records = []
    for node in normal_ids:
        sub = merged[merged["node"] == node].sort_values("time_send")
        n = len(sub)
        total = math.ceil(n / series_size)
        healthy = 0
        for i in range(total):
            chunk = sub.iloc[i*series_size : (i+1)*series_size]
            if chunk["recv_node"].isin(spine_ids).any():
                healthy += 1
        frac = healthy / total if total > 0 else 0.0
        records.append([node, total, healthy, frac])

    dfh = pd.DataFrame(
        records,
        columns=["node", "total_series", "healthy_series", "health_fraction"]
    ).set_index("node")
    dfh["health_fraction"] = dfh["health_fraction"].map("{:.2%}".format)
    return dfh

def compute_health_over_time(
    merged: pd.DataFrame,
    normal_ids,
    spine_ids,
    series_size: int,
    t0: float,
    t1: float,
    steps: int = 10
):
    """
    Returns a list of (percent, health_fraction) at each of 10%,20%,…100% of sim time.
    """
    results = []
    for step in range(1, steps + 1):
        pct = step * 100 // steps
        t_cut = t0 + (t1 - t0) * (pct / 100.0)
        window = merged[merged["time_send"] <= t_cut]

        total = healthy = 0
        for node in normal_ids:
            sub = window[window["node"] == node].sort_values("time_send")
            cnt = math.ceil(len(sub) / series_size)
            total += cnt
            for i in range(cnt):
                chunk = sub.iloc[i*series_size : (i+1)*series_size]
                if chunk["recv_node"].isin(spine_ids).any():
                    healthy += 1

        frac = healthy / total if total > 0 else 0.0
        results.append((pct, frac))

    return results

def analyze_health(path: str, series_size: int, steps: int = 10):
    # load & prepare
    df_send, merged, normal_ids, spine_ids, t0, t1 = load_and_merge_packets(path)

    if series_size is None:
        series_size = infer_series_size_from_runs(df_send)
    print(f"\nInferred series size = {series_size}\n")

    # static per-node health
    dfh = compute_health(merged, normal_ids, spine_ids, series_size)
    print("=== NETWORK HEALTH ===")
    print(dfh.to_string())
    out1 = os.path.splitext(path)[0] + "_health_per_node.csv"
    dfh.to_csv(out1)
    print(f"Health per node written to {out1}\n")

    # over-time health
    results = compute_health_over_time(
        merged, normal_ids, spine_ids, series_size, t0, t1, steps
    )
    print("=== NETWORK HEALTH OVER TIME ===")
    for pct, frac in results:
        print(f"[{pct:3d}%] -> {frac:.2%}")
    out2 = os.path.splitext(path)[0] + "_health_over_time.csv"
    pd.DataFrame(results, columns=["percent","health_fraction"]).to_csv(out2, index=False)
    print(f"\nHealth over time written to {out2}\n")


def analyze_movement(path: str):
    df = pd.read_csv(path)
    print("=== MOVEMENT STATISTICS ===")
    tmin, tmax = df["time"].min(), df["time"].max()
    print(f"Times: {len(df['time'].unique())} points, duration {tmax-tmin:.2f}s")
    print(f"Nodes: {len(df['node'].unique())}")
    print(f"X range: {df['x'].min():.2f}-{df['x'].max():.2f}, Y range: {df['y'].min():.2f}-{df['y'].max():.2f}")
    print(f"Speed mean/std/min/max: {df['speed'].mean():.3f}/{df['speed'].std():.3f}/{df['speed'].min():.3f}/{df['speed'].max():.3f}")


def analyze_connectivity(path: str, nodes_per_group: int = 5):
    df = pd.read_csv(path)
    print("\n=== L2 CONNECTIVITY SUMMARY ===\n")
    if "l2_link" not in df.columns:
        raise RuntimeError("Expected a 'l2_link' column for connectivity data")

    # mark each sample as "online" if any l2_link > 0
    df["online"] = df["l2_link"].astype(int) > 0

    # overall summary
    overall = df["online"].mean()
    print(f"Overall neighbour visibility fraction (if node sees ANY other node): {overall:.2%}\n")

    # per-node: count total, sum online, mean fraction
    per_node = (
        df
        .groupby("node")["online"]
        .agg(
            total_samples="count",
            online_count="sum",
            visibility_fraction="mean"
        )
        .sort_index()
    )
    # format the fraction column as percent
    per_node["visibility_fraction"] = per_node["visibility_fraction"].map("{:.2%}".format)

    nodes = per_node.index.tolist()
    for i in range(0, len(nodes), nodes_per_group):
        group = nodes[i : i + nodes_per_group]
        panel = per_node.loc[group].T
        print(f"Nodes {group[0]}–{group[-1]}:")
        print(panel.to_string())
        print()


def plot_movement(
    movement_path: str,
    connectivity_path: str,
    output_path: str,
    mark_offline: bool = True,
    x_max=None,
    y_max=None
):
    # Load both CSVs
    df_move = pd.read_csv(movement_path)
    df_conn = pd.read_csv(connectivity_path)

    # First offline time per node
    offline_times = (
        df_conn[df_conn["online"] == False]
               .groupby("node")["time"]
               .min()
               .reset_index()
               .rename(columns={"time":"offline_time"})
    )

    unique = df_move["node"].unique()
    plt.figure(figsize=(8,6))

    for node_label in unique:
        sub = df_move[df_move["node"]==node_label].sort_values("time")
        x, y = sub["x"].values, sub["y"].values
        c = "#e63946" if str(node_label).endswith("S") else "#8c8c8c"

        # Draw start point
        plt.scatter(x[0], y[0], color=c, s=80, edgecolor="k")
        plt.scatter(x[0], y[0], color=c, s=80, edgecolor="k", zorder=4)
        # Annotate the node number with a shadowed text on top
        txt = plt.text(
            x[0], y[0], str(node_label),
            ha="center", va="center",
            fontsize=8, fontweight="bold",
            color="white", zorder=10
        )
        # add a black outline / shadow
        txt.set_path_effects([
            path_effects.Stroke(linewidth=2, foreground='black'),
            path_effects.Normal()
        ])

        # Determine offline index if marking
        offline_idx = None
        if mark_offline:
            try:
                nid = int(str(node_label).rstrip("S"))
            except ValueError:
                nid = None
            if nid is not None:
                off = offline_times[offline_times["node"]==nid]
                if not off.empty:
                    t_off = off["offline_time"].iloc[0]
                    nearest = sub.iloc[(sub["time"]-t_off).abs().argmin()]
                    plt.scatter(
                        nearest["x"], nearest["y"],
                        marker="x", s=80,
                        c="green", linewidths=2, zorder=5
                    )
                    offline_idx = sub.index.get_loc(nearest.name)

        # Clip trajectory if offline
        end = offline_idx+1 if (mark_offline and offline_idx is not None) else len(x)
        # Draw trajectory up to end
        pts_x, pts_y = x[:end], y[:end]
        plt.scatter(pts_x[1:], pts_y[1:], color=c, alpha=0.6)
        for i in range(end-1):
            plt.arrow(
                pts_x[i], pts_y[i],
                pts_x[i+1]-pts_x[i], pts_y[i+1]-pts_y[i],
                head_width=0.2, head_length=0.5,
                length_includes_head=True,
                fc=c, ec=c, alpha=0.5
            )

    title = "Movement Plot"
    if mark_offline:
        title += " (× marks down state)"
    plt.title(title)
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.grid(True)
    if x_max is not None:
        plt.xlim(0, x_max)
    if y_max is not None:
        plt.ylim(0, y_max)
    plt.savefig(output_path, dpi=300)
    plt.close()

def main():
    parser = argparse.ArgumentParser(description="Unified MANET analysis")
    parser.add_argument("--packets", help="packets.csv path")
    parser.add_argument("--nodes",   type=int,   help="number of numeric nodes")
    parser.add_argument("--series",  type=int,   help="series size for availability calc")
    parser.add_argument("--movement",help="movement.csv path")
    parser.add_argument("--connectivity", help="connectivity.csv path")
    parser.add_argument("--plot",     help="output path for movement plot")
    parser.add_argument("--xmax",     type=float, default=None)
    parser.add_argument("--ymax",     type=float, default=None)
    parser.add_argument("--no-mark-offline",
                        action="store_true",
                        help="Disable × markers and stop path at offline")
    args = parser.parse_args()

    if args.packets:
        analyze_health(
            args.packets,
            steps=10,
            series_size=args.series
        )

    if args.movement:
        analyze_movement(args.movement)

    if args.connectivity:
        analyze_connectivity(args.connectivity)

    if args.plot and args.movement:
        if not args.connectivity:
            raise RuntimeError("`--plot` requires `--connectivity` for offline markers")
        plot_movement(
            args.movement,
            args.connectivity,
            args.plot,
            mark_offline=not args.no_mark_offline,
            x_max=args.xmax,
            y_max=args.ymax
        )

if __name__ == "__main__":
    main()
