import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import os # For checking if files exist

def plot_latency_cdf(epoll_file, io_uring_file, output_image_name="latency_cdf_comparison.png",
                     x_label="Latency (nanoseconds)", y_label="Cumulative Probability (%)",
                     x_lim=None, y_lim=None, title="Latency Distribution: Epoll vs. io_uring"):
    """
    Reads latency data from specified files and plots their Cumulative Distribution Functions (CDFs).

    Args:
        epoll_file (str): Path to the file containing Epoll server latencies.
        io_uring_file (str): Path to the file containing io_uring server latencies.
        output_image_name (str): Name of the file to save the plot (e.g., 'latency_cdf_comparison.png').
        x_label (str): Label for the X-axis.
        y_label (str): Label for the Y-axis.
        x_lim (tuple, optional): A tuple (min_x, max_x) to set the X-axis limits. If None, auto-scales.
        y_lim (tuple, optional): A tuple (min_y, max_y) to set the Y-axis limits. If None, auto-scales.
        title (str): Title of the plot.
    """

    latencies_epoll = []
    latencies_io_uring = []

    # Read Epoll latencies
    if os.path.exists(epoll_file):
        try:
            with open(epoll_file, 'r') as f:
                for line in f:
                    try:
                        latencies_epoll.append(float(line.strip()))
                    except ValueError:
                        print(f"Warning: Could not convert line to float in {epoll_file}: {line.strip()}")
            print(f"Read {len(latencies_epoll)} latencies from {epoll_file}")
        except Exception as e:
            print(f"Error reading {epoll_file}: {e}")
            return
    else:
        print(f"Error: Epoll latency file not found at {epoll_file}")
        return

    # Read io_uring latencies
    if os.path.exists(io_uring_file):
        try:
            with open(io_uring_file, 'r') as f:
                for line in f:
                    try:
                        latencies_io_uring.append(float(line.strip()))
                    except ValueError:
                        print(f"Warning: Could not convert line to float in {io_uring_file}: {line.strip()}")
            print(f"Read {len(latencies_io_uring)} latencies from {io_uring_file}")
        except Exception as e:
            print(f"Error reading {io_uring_file}: {e}")
            return
    else:
        print(f"Error: io_uring latency file not found at {io_uring_file}")
        return

    if not latencies_epoll or not latencies_io_uring:
        print("Not enough data to plot. Ensure both files contain latency values.")
        return

    # Convert lists to numpy arrays for efficient processing
    latencies_epoll = np.array(latencies_epoll)
    latencies_io_uring = np.array(latencies_io_uring)

    plt.figure(figsize=(12, 7)) # Adjust figure size for better readability

    # Plotting CDFs using seaborn's ecdfplot (Empirical CDF)
    # This automatically handles sorting and cumulative percentages
    sns.ecdfplot(latencies_epoll, label='Epoll Server', color='blue', linestyle='-', linewidth=2)
    # FIX: Corrected 'linewidth2' to 'linewidth=2'
    sns.ecdfplot(latencies_io_uring, label='io_uring Server', color='red', linestyle='-', linewidth=2)

    # Calculate and display key percentiles (P90, P95, P99)
    # This adds numerical clarity to the visual comparison
    percentiles_to_mark = [90, 95, 99, 99.9] # Add 99.9 for very high-performance systems

    for p in percentiles_to_mark:
        p_val_epoll = np.percentile(latencies_epoll, p)
        p_val_io_uring = np.percentile(latencies_io_uring, p)

        # Add vertical lines to mark percentiles on the graph
        plt.axvline(x=p_val_epoll, color='blue', linestyle=':', linewidth=1, alpha=0.7)
        plt.axvline(x=p_val_io_uring, color='red', linestyle=':', linewidth=1, alpha=0.7)

        # Add text annotations for clarity (adjust positions if they overlap)
        plt.text(p_val_epoll, p/100.0, f'E_P{p}:{p_val_epoll:.1f}', color='blue', ha='right', va='bottom', fontsize=9)
        plt.text(p_val_io_uring, p/100.0, f'IOU_P{p}:{p_val_io_uring:.1f}', color='red', ha='left', va='top', fontsize=9)

        print(f"P{p} Latency - Epoll: {p_val_epoll:.2f} ns, io_uring: {p_val_io_uring:.2f} ns")

    # Add a horizontal line for 99% percentile to visually align
    # plt.axhline(y=0.99, color='gray', linestyle='--', linewidth=0.8, label='99th Percentile')


    # Set labels and title
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(title)

    # Format Y-axis to show percentages
    plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(lambda y, _: f'{y*100:.0f}%'))

    # Option to specify X-axis and Y-axis scale (range)
    if x_lim:
        plt.xlim(x_lim)
    if y_lim:
        plt.ylim(y_lim)

    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend(loc='lower right', frameon=True) # Add frame for better visibility
    plt.tight_layout() # Adjust layout to prevent labels from overlapping

    # Save the plot
    plt.savefig(output_image_name, dpi=300) # Save with high resolution
    plt.show() # Display the plot

if __name__ == "__main__":
    # --- Configuration ---
    EPOLL_LATENCY_FILE = "latencies_epoll.txt"
    IO_URING_LATENCY_FILE = "latencies_io_uring.txt"
    OUTPUT_PLOT_FILE = "latency_cdf_comparison.png" # You can change this
    X_AXIS_LABEL = "Latency (nanoseconds)"
    Y_AXIS_LABEL = "Cumulative Probability (%)"
    PLOT_TITLE = "Latency Distribution: Epoll vs. io_uring (Fixed Clients)"

    # --- How to specify X-axis range (uncomment and adjust as needed) ---
    # For example, if most latencies are between 0 and 500 ns, but you want to focus on the 0-200 ns range:
    # X_AXIS_RANGE = (0, 200) # Min and Max values for X-axis
    X_AXIS_RANGE = None # Set to None for auto-scaling

    # --- How to specify Y-axis range (usually 0 to 1 for CDFs, but you can zoom if needed) ---
    # Y_AXIS_RANGE = (0, 1) # Standard for CDF
    Y_AXIS_RANGE = None # Set to None for auto-scaling


    plot_latency_cdf(
        epoll_file=EPOLL_LATENCY_FILE,
        io_uring_file=IO_URING_LATENCY_FILE,
        output_image_name=OUTPUT_PLOT_FILE,
        x_label=X_AXIS_LABEL,
        y_label=Y_AXIS_LABEL,
        x_lim=X_AXIS_RANGE, # Pass the custom range here
        y_lim=Y_AXIS_RANGE,
        title=PLOT_TITLE
    )