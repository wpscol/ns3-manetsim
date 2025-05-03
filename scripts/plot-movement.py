#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import argparse


# Step 1: Set up argument parsing
parser = argparse.ArgumentParser(
    description="Plot x and y from a CSV file for each node and save the plot to a specified file."
)
parser.add_argument(
    "--output_path",
    type=str,
    help="Path to save the output plot (e.g., ./output/scatter_plot.png)",
)
parser.add_argument(
    "--input_path",
    type=str,
    help="Path to load the data from (e.g., ./data/movement.csv)",
)
parser.add_argument(
    "--x_max", type=float, help="Maximum value for X axis", default=None
)
parser.add_argument(
    "--y_max", type=float, help="Maximum value for Y axis", default=None
)

# Step 2: Parse the arguments
args = parser.parse_args()

# Step 3: Read the CSV file
data = pd.read_csv(args.input_path)

# Step 4: Get unique nodes and split into normal vs. spine
unique_nodes = list(data["node"].unique())
spine_nodes = [n for n in unique_nodes if str(n).endswith("S")]
normal_nodes = [n for n in unique_nodes if n not in spine_nodes]

# Step 5: Define two fixed colors
normal_color = "#8c8c8c"  # medium‑dark gray
spine_color = "#e63946"  # vibrant red‑orange

# Step 6: Create the plot
plt.figure(figsize=(10, 6))

# Step 7: Plot each node and draw arrows for transitions
for node in unique_nodes:
    node_data = data[data["node"] == node]
    x = node_data["x"].values
    y = node_data["y"].values

    # pick the right color
    color = spine_color if node in spine_nodes else normal_color

    # highlight the first position more prominently
    plt.scatter(x[0], y[0], color=color, s=100, edgecolor="k", linewidth=1.5)

    # scatter the rest of the positions
    plt.scatter(x[1:], y[1:], color=color, alpha=0.7)

    # connect them with arrows
    for i in range(len(x) - 1):
        plt.arrow(
            x[i],
            y[i],
            x[i + 1] - x[i],
            y[i + 1] - y[i],
            head_width=0.2,
            head_length=0.5,
            length_includes_head=True,
            fc=color,
            ec=color,
            alpha=0.5,
        )

# Step 8: Customize the plot
plt.title("General soldier movement (spine nodes highlighted)")
plt.xlabel("X")
plt.ylabel("Y")
plt.grid(True)

# Step 9: Apply optional axis limits
if args.x_max is not None:
    plt.xlim(0, args.x_max)
if args.y_max is not None:
    plt.ylim(0, args.y_max)

# Step 10: Save and close
plt.savefig(args.output_path, dpi=300)
plt.close()
