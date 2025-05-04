#!/usr/bin/env python3
"""
analyze_all.py

One-stop analysis for MANET simulation outputs:
  - Packet QoS metrics (PDR, delay, throughput)
  - Mobility statistics (speed, distance, bounding box)
  - Connectivity summary (online fraction)
  - Optional movement plot

Splits into key functions:
  * analyze_packets(file, nodes)
  * analyze_movement(file)
  * analyze_connectivity(file)
  * plot_movement(input_path, output_path, x_max, y_max)

Usage:
  python3 analyze_all.py \
    --packets packets.csv --nodes 10 \
    --movement movement.csv \
    --connectivity connectivity.csv \
    [--plot output.png --xmax 50 --ymax 50]
"""
import argparse
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def analyze_packets(path: str, num_nodes: int):
    df = pd.read_csv(path, dtype={"node": str})
    # validate
    for col in ("node", "time", "uid", "size", "received"):
        if col not in df.columns:
            raise ValueError(f"Missing column {col} in packets CSV")
    # normalize
    df["received"] = (
        pd.to_numeric(df["received"], errors="coerce").fillna(0).astype(int)
    )
    df_send = df[df["received"] == 0]
    df_recv = df[df["received"] == 1]
    tmin, tmax = df["time"].min(), df["time"].max()
    duration = tmax - tmin
    # ids
    spine_ids = sorted(df["node"][df["node"].str.endswith("S")].unique())
    if num_nodes is not None:
        normal_ids = [str(i) for i in range(num_nodes) if f"{i}S" not in spine_ids]
    else:
        normal_ids = sorted(df["node"][~df["node"].str.endswith("S")].unique())
    all_ids = normal_ids + spine_ids
    # merge for delay
    merged = pd.merge(
        df_send[["uid", "time", "node", "size"]],
        df_recv[["uid", "time"]],
        on="uid",
        how="left",
        suffixes=("_send", "_recv"),
    )
    merged["delay"] = merged["time_recv"] - merged["time_send"]
    delivered = merged.dropna(subset=["time_recv"])
    # compute
    records = []
    for node in all_ids:
        sent = df_send[df_send["node"] == node]
        recv = df_recv[df_recv["node"] == node]
        ts, tr = len(sent), len(recv)
        td = len(delivered[delivered["node"] == node])
        pdr = td / ts if ts > 0 else 0.0
        delays = delivered[delivered["node"] == node]["delay"]
        avg_d = delays.mean() if not delays.empty else 0.0
        min_d = delays.min() if not delays.empty else 0.0
        max_d = delays.max() if not delays.empty else 0.0
        jitter = delays.std() if not delays.empty else 0.0
        bs = sent["size"].sum()
        br = recv["size"].sum()
        thr = (br * 8) / duration if duration > 0 else 0.0
        marker = "*" if node.endswith("S") else ""
        name = node[:-1] if node.endswith("S") else node
        records.append(
            [marker, name, ts, tr, pdr, avg_d, min_d, max_d, jitter, bs, br, thr]
        )
    cols = [
        "marker",
        "node",
        "sent_pkts",
        "recv_pkts",
        "pdr",
        "avg_delay_s",
        "min_delay_s",
        "max_delay_s",
        "jitter_s",
        "bytes_sent",
        "bytes_recv",
        "throughput_bps",
    ]
    dfm = pd.DataFrame(records, columns=cols).set_index("node")
    dfm = dfm.sort_index(key=lambda idx: idx.astype(int))
    # summary
    print("\n=== PACKET QoS METRICS ===")
    total_sent = len(df_send)
    total_deliv = len(delivered)
    overall_pdr = total_deliv / total_sent if total_sent > 0 else 0.0
    overall_delay = delivered["delay"].mean() if not delivered.empty else 0.0
    overall_thr = (delivered["size"].sum() * 8) / duration if duration > 0 else 0.0
    print(f"Duration: {tmin:.3f}-{tmax:.3f}s ({duration:.3f}s)")
    print(f"Total sent: {total_sent}, delivered: {total_deliv}, PDR: {overall_pdr:.1%}")
    print(f"Avg delay: {overall_delay:.3f}s, Throughput: {overall_thr/1e6:.3f} Mbps")
    print(dfm.to_string(float_format="%.3f"))
    # write
    out = os.path.splitext(path)[0] + "_metrics_per_node.csv"
    dfm.to_csv(out)
    print(f"Metrics written to {out}\n")


def analyze_movement(path: str):
    df = pd.read_csv(path)
    print("=== MOVEMENT STATISTICS ===")
    tmin, tmax = df["time"].min(), df["time"].max()
    print(f"Times: {len(df['time'].unique())} points, duration {tmax-tmin:.2f}s")
    print(f"Nodes: {len(df['node'].unique())}")
    print(
        f"X range: {df['x'].min():.2f}-{df['x'].max():.2f}, Y range: {df['y'].min():.2f}-{df['y'].max():.2f}"
    )
    print(
        f"Speed mean/std/min/max: {df['speed'].mean():.3f}/{df['speed'].std():.3f}/{df['speed'].min():.3f}/{df['speed'].max():.3f}"
    )
    # distance


def total_distance(df):
    d = np.sqrt(
        df["x"].diff().fillna(0) ** 2
        + df["y"].diff().fillna(0) ** 2
        + df["z"].diff().fillna(0) ** 2
    )
    print("Per-node distance traveled:")
    dists = {
        node: total_distance(sub.sort_values("time"))
        for node, sub in df.groupby("node")
    }
    sdist = pd.Series(dists).sort_values()
    print(sdist.to_string())
    return d.sum()


def analyze_connectivity(path: str):
    df = pd.read_csv(path)
    print("\n=== CONNECTIVITY SUMMARY ===")
    # Expect 'link' column indicating number of active links
    if "link" not in df.columns:
        raise RuntimeError("Expected a 'link' column for connectivity data")
    # Consider node 'online' if link count > 0
    df["online"] = df["link"].astype(int) > 0

    # Overall fraction of time nodes are online
    overall = df["online"].mean()
    print(f"Overall online fraction: {overall:.2%}")

    # Per-node online fraction
    per_node = df.groupby("node")["online"].mean().sort_index()
    print("Per-node online fraction:")
    print(per_node.to_string())


def plot_movement(input_path: str, output_path: str, x_max=None, y_max=None):
    df = pd.read_csv(input_path)
    unique = df["node"].unique()
    spine = [n for n in unique if str(n).endswith("S")]
    normal = [n for n in unique if n not in spine]
    plt.figure(figsize=(8, 6))
    for node in unique:
        sub = df[df["node"] == node]
        x, y = sub["x"], sub["y"]
        c = "#e63946" if node in spine else "#8c8c8c"
        plt.scatter(x.iloc[0], y.iloc[0], color=c, s=80, edgecolor="k")
        plt.scatter(x.iloc[1:], y.iloc[1:], color=c, alpha=0.6)
        for i in range(len(x) - 1):
            plt.arrow(
                x.iat[i],
                y.iat[i],
                x.iat[i + 1] - x.iat[i],
                y.iat[i + 1] - y.iat[i],
                head_width=0.2,
                head_length=0.5,
                length_includes_head=True,
                fc=c,
                ec=c,
                alpha=0.5,
            )
    plt.title("Movement Plot")
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.grid(True)
    if x_max:
        plt.xlim(0, x_max)
    if y_max:
        plt.ylim(0, y_max)
    plt.savefig(output_path, dpi=300)
    plt.close()


def main():
    p = argparse.ArgumentParser(description="Unified MANET analysis")
    p.add_argument("--packets", help="packets.csv path")
    p.add_argument("--nodes", type=int, help="number of numeric nodes")
    p.add_argument("--movement", help="movement.csv path")
    p.add_argument("--connectivity", help="connectivity.csv path")
    p.add_argument("--plot", help="output path for movement plot")
    p.add_argument("--xmax", type=float, default=None)
    p.add_argument("--ymax", type=float, default=None)
    args = p.parse_args()
    if args.packets:
        analyze_packets(args.packets, args.nodes)
    if args.movement:
        analyze_movement(args.movement)
    if args.connectivity:
        analyze_connectivity(args.connectivity)
    if args.plot and args.movement:
        plot_movement(args.movement, args.plot, args.xmax, args.ymax)


if __name__ == "__main__":
    main()
