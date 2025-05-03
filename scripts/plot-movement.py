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

# Step 4: Get unique nodes
unique_nodes = data["node"].unique()

# Step 5: Create the plot
plt.figure(figsize=(10, 6))

# Step 6: Define a color map for nodes using the updated method
colors = plt.colormaps["tab10"]

# Step 7: Plot each node and draw arrows for transitions
for idx, node in enumerate(unique_nodes):
    node_data = data[data["node"] == node]
    x = node_data["x"]
    y = node_data["y"]

    # Highlight the first value of each node
    plt.scatter(x.iloc[0], y.iloc[0], color="orange", s=100)  # Highlight first point

    # Scatter plot for the rest of the node values
    plt.scatter(x.iloc[1:], y.iloc[1:], color=colors(idx))

    # Draw arrows between states within the same node
    for i in range(len(node_data) - 1):
        x_start = node_data["x"].iloc[i]
        y_start = node_data["y"].iloc[i]
        x_end = node_data["x"].iloc[i + 1]
        y_end = node_data["y"].iloc[i + 1]

        # Draw the arrow
        plt.arrow(
            x_start,
            y_start,
            x_end - x_start,
            y_end - y_start,
            head_width=0.2,
            head_length=0.5,
            fc=colors(idx),
            ec=colors(idx),
            alpha=0.5,
        )

# Step 8: Customize the plot
plt.title(
    "Scatter Plot of x vs y for All Nodes with Highlighted First Values and History"
)
plt.xlabel("x values")
plt.ylabel("y values")
plt.grid(True)

# plt.legend()  # Removed the legend

# Step 9: Set X and Y axis limits
plt.xlim(0, args.x_max if args.x_max is not None else None)
plt.ylim(0, args.y_max if args.y_max is not None else None)

# Step 10: Save the plot to the specified file
plt.savefig(args.output_path)

# Optional: Close the plot to free up memory
plt.close()
