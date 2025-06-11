import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import os # For checking if files exist

def plot_latency_cdf(epoll_file, io_uring_file, output_image_name="latency_cdf_comparison.png",
                     x_label="Latency (nanoseconds)", y_label="Cumulative Probability (%)",
                     x_lim=None, y_lim=None, title="Latency Distribution: Epoll vs. io_uring"):
    """
    Reads latency data from specified files, filters points outside x_lim,
    and plots their Cumulative Distribution Functions (CDFs).

    Args:
        epoll_file (str): Path to the file containing Epoll server latencies.
        io_uring_file (str): Path to the file containing io_uring server latencies.
        output_image_name (str): Name of the file to save the plot (e.g., 'latency_cdf_comparison.png').
        x_label (str): Label for the X-axis.
        y_label (str): Label for the Y-axis.
        x_lim (tuple, optional): A tuple (min_x, max_x) to set the X-axis limits AND filter data.
                                 If None, no filtering or specific x-axis limits are applied.
        y_lim (tuple, optional): A tuple (min_y, max_y) to set the Y-axis limits. If None, auto-scales.
        title (str): Title of the plot.
    """

    def read_latencies_from_file(filepath):
        latencies = []
        if os.path.exists(filepath):
            try:
                with open(filepath, 'r') as f:
                    for line in f:
                        try:
                            latencies.append(float(line.strip()))
                        except ValueError:
                            print(f"Warning: Could not convert line to float in {filepath}: {line.strip()}")
                print(f"Read {len(latencies)} raw latencies from {filepath}")
            except Exception as e:
                print(f"Error reading {filepath}: {e}")
                return []
        else:
            print(f"Error: Latency file not found at {filepath}")
            return []
        return np.array(latencies)

    latencies_epoll_raw = read_latencies_from_file(epoll_file)
    latencies_io_uring_raw = read_latencies_from_file(io_uring_file)

    if not latencies_epoll_raw.size or not latencies_io_uring_raw.size:
        print("Not enough data to plot after reading. Ensure both files contain latency values.")
        return

    # --- Filtering Logic ---
    latencies_epoll_filtered = latencies_epoll_raw
    latencies_io_uring_filtered = latencies_io_uring_raw

    if x_lim and len(x_lim) == 2:
        min_x, max_x = x_lim
        # Filter datapoints that are within the specified x_lim range
        latencies_epoll_filtered = latencies_epoll_raw[(latencies_epoll_raw >= min_x) & (latencies_epoll_raw <= max_x)]
        latencies_io_uring_filtered = latencies_io_uring_raw[(latencies_io_uring_raw >= min_x) & (latencies_io_uring_raw <= max_x)]
        print(f"Epoll: {len(latencies_epoll_raw) - len(latencies_epoll_filtered)} points filtered out from {len(latencies_epoll_raw)} raw points.")
        print(f"io_uring: {len(latencies_io_uring_raw) - len(latencies_io_uring_filtered)} points filtered out from {len(latencies_io_uring_raw)} raw points.")
    else:
        print("No x_lim specified or x_lim is invalid. Plotting all data points.")


    if not latencies_epoll_filtered.size or not latencies_io_uring_filtered.size:
        print("Not enough data to plot after filtering. Adjust x_lim or check data.")
        return

    plt.figure(figsize=(12, 7))

    # Plotting CDFs using seaborn's ecdfplot
    sns.ecdfplot(latencies_epoll_filtered, label='Epoll Server', color='blue', linestyle='-', linewidth=2)
    sns.ecdfplot(latencies_io_uring_filtered, label='io_uring Server', color='red', linestyle='-', linewidth=2)

    # Calculate and display key percentiles (P90, P95, P99) based on the FILTERED data
    percentiles_to_mark = [90, 95, 99, 99.9]

    for p in percentiles_to_mark:
        if latencies_epoll_filtered.size > 0: # Ensure there's data to calculate percentile
            p_val_epoll = np.percentile(latencies_epoll_filtered, p)
            plt.axvline(x=p_val_epoll, color='blue', linestyle=':', linewidth=1, alpha=0.7)
            plt.text(p_val_epoll, p/100.0, f'E_P{p}:{p_val_epoll:.1f}', color='blue', ha='right', va='bottom', fontsize=9)
            print(f"P{p} Latency (Filtered) - Epoll: {p_val_epoll:.2f} ns")
        else:
            print(f"Not enough filtered Epoll data for P{p} percentile.")

        if latencies_io_uring_filtered.size > 0: # Ensure there's data to calculate percentile
            p_val_io_uring = np.percentile(latencies_io_uring_filtered, p)
            plt.axvline(x=p_val_io_uring, color='red', linestyle=':', linewidth=1, alpha=0.7)
            plt.text(p_val_io_uring, p/100.0, f'IOU_P{p}:{p_val_io_uring:.1f}', color='red', ha='left', va='top', fontsize=9)
            print(f"P{p} Latency (Filtered) - io_uring: {p_val_io_uring:.2f} ns")
        else:
            print(f"Not enough filtered io_uring data for P{p} percentile.")

    # Set labels and title
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(title)

    # Format Y-axis to show percentages
    plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(lambda y, _: f'{y*100:.0f}%'))

    # Apply X-axis and Y-axis limits
    # Note: x_lim is now used for both filtering and display range if specified.
    if x_lim:
        plt.xlim(x_lim)
    if y_lim:
        plt.ylim(y_lim)

    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend(loc='lower right', frameon=True)
    plt.tight_layout()

    # Save the plot
    plt.savefig(output_image_name, dpi=300)
    plt.show()

if __name__ == "__main__":
    # --- Configuration ---
    EPOLL_LATENCY_FILE = "latencies_epoll.txt"
    IO_URING_LATENCY_FILE = "latencies_io_uring.txt"
    OUTPUT_PLOT_FILE = "latency_cdf_filtered_comparison.png" # Renamed output file
    X_AXIS_LABEL = "Latency (nanoseconds)"
    Y_AXIS_LABEL = "Cumulative Probability (%)"
    PLOT_TITLE = "Latency Distribution: Epoll vs. io_uring (Filtered Range)"

    # --- How to specify X-axis range for plotting AND DATA FILTERING ---
    # To filter and plot data only between 0 and 500 nanoseconds:
    X_AXIS_RANGE = (0, 50)
    # Set to None for auto-scaling and no data filtering based on x_lim
    # X_AXIS_RANGE = None

    # --- How to specify Y-axis range (usually 0 to 1 for CDFs) ---
    Y_AXIS_RANGE = (0, 1) # Standard for CDF
    # Set to None for auto-scaling
    # Y_AXIS_RANGE = None

    plot_latency_cdf(
        epoll_file=EPOLL_LATENCY_FILE,
        io_uring_file=IO_URING_LATENCY_FILE,
        output_image_name=OUTPUT_PLOT_FILE,
        x_label=X_AXIS_LABEL,
        y_label=Y_AXIS_LABEL,
        x_lim=X_AXIS_RANGE, # This will now filter the data AND set the plot limits
        y_lim=Y_AXIS_RANGE,
        title=PLOT_TITLE
    )