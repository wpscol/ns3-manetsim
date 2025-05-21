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
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def analyze_packets(path: str, num_nodes: int):
    df = pd.read_csv(path, dtype={"node": str})
    for col in ("node", "time", "uid", "size", "received"):
        if col not in df.columns:
            raise ValueError(f"Missing column {col} in packets CSV")
    df["received"] = pd.to_numeric(df["received"], errors="coerce").fillna(0).astype(int)
    df_send = df[df["received"] == 0]
    df_recv = df[df["received"] == 1]
    tmin, tmax = df["time"].min(), df["time"].max()
    duration = tmax - tmin

    spine_ids = sorted(df["node"][df["node"].str.endswith("S")].unique())
    if num_nodes is not None:
        normal_ids = [str(i) for i in range(num_nodes) if f"{i}S" not in spine_ids]
    else:
        normal_ids = sorted(df["node"][~df["node"].str.endswith("S")].unique())
    all_ids = normal_ids + spine_ids

    merged = pd.merge(
        df_send[["uid","time","node","size"]],
        df_recv[["uid","time"]],
        on="uid", how="left", suffixes=("_send","_recv")
    )
    merged["delay"] = merged["time_recv"] - merged["time_send"]
    delivered = merged.dropna(subset=["time_recv"])

    records = []
    for node in all_ids:
        sent = df_send[df_send["node"] == node]
        recv = df_recv[df_recv["node"] == node]
        ts, tr = len(sent), len(recv)
        td = len(delivered[delivered["node"] == node])
        pdr = td/ts if ts>0 else 0.0
        delays = delivered[delivered["node"] == node]["delay"]
        avg_d = delays.mean() if not delays.empty else 0.0
        min_d = delays.min()  if not delays.empty else 0.0
        max_d = delays.max()  if not delays.empty else 0.0
        jitter = delays.std() if not delays.empty else 0.0
        bs = sent["size"].sum()
        br = recv["size"].sum()
        thr = (br*8)/duration if duration>0 else 0.0
        marker = "*" if node.endswith("S") else ""
        name = node[:-1] if node.endswith("S") else node
        records.append([marker,name,ts,tr,pdr,avg_d,min_d,max_d,jitter,bs,br,thr])

    cols = ["marker","node","sent_pkts","recv_pkts","pdr",
            "avg_delay_s","min_delay_s","max_delay_s","jitter_s",
            "bytes_sent","bytes_recv","throughput_bps"]
    dfm = pd.DataFrame(records, columns=cols).set_index("node")
    dfm = dfm.sort_index(key=lambda idx: idx.astype(int))

    print("\n=== PACKET QoS METRICS ===")
    total_sent = len(df_send)
    total_deliv = len(delivered)
    overall_pdr = total_deliv/total_sent if total_sent>0 else 0.0
    overall_delay = delivered["delay"].mean() if not delivered.empty else 0.0
    overall_thr = (delivered["size"].sum()*8)/duration if duration>0 else 0.0
    print(f"Duration: {tmin:.3f}-{tmax:.3f}s ({duration:.3f}s)")
    print(f"Total sent: {total_sent}, delivered: {total_deliv}, PDR: {overall_pdr:.1%}")
    print(f"Avg delay: {overall_delay:.3f}s, Throughput: {overall_thr/1e6:.3f} Mbps")
    print(dfm.to_string(float_format="%.3f"))

    out = os.path.splitext(path)[0] + "_metrics_per_node.csv"
    dfm.to_csv(out)
    print(f"Metrics written to {out}\n")

def analyze_movement(path: str):
    df = pd.read_csv(path)
    print("=== MOVEMENT STATISTICS ===")
    tmin, tmax = df["time"].min(), df["time"].max()
    print(f"Times: {len(df['time'].unique())} points, duration {tmax-tmin:.2f}s")
    print(f"Nodes: {len(df['node'].unique())}")
    print(f"X range: {df['x'].min():.2f}-{df['x'].max():.2f}, Y range: {df['y'].min():.2f}-{df['y'].max():.2f}")
    print(f"Speed mean/std/min/max: {df['speed'].mean():.3f}/{df['speed'].std():.3f}/{df['speed'].min():.3f}/{df['speed'].max():.3f}")

def analyze_connectivity(path: str):
    df = pd.read_csv(path)
    print("\n=== CONNECTIVITY SUMMARY ===")
    if "link" not in df.columns:
        raise RuntimeError("Expected a 'link' column for connectivity data")
    df["online"] = df["link"].astype(int) > 0
    overall = df["online"].mean()
    print(f"Overall online fraction: {overall:.2%}")
    per_node = df.groupby("node")["online"].mean().sort_index()
    print("Per-node online fraction:")
    print(per_node.to_string())

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
        analyze_packets(args.packets, args.nodes)
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
