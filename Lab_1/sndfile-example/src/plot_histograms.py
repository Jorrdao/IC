import matplotlib.pyplot as plt
import numpy as np
import sys

def plot_histogram(filename, title):
    # Read the data from the file
    data = np.loadtxt(filename)
    
    # Separate into x (sample values) and y (counts)
    x = data[:, 0]
    y = data[:, 1]
    
    # Create the plot
    plt.figure(figsize=(12, 6))
    # Calculate appropriate bar width
    if len(x) > 1:
        min_gap = np.min(np.diff(np.sort(x)))
        bar_width = min_gap * 0.8
    else:
        bar_width = 1000

    plt.bar(x, y, width=bar_width)

    if len(x) > 0:
        x_min, x_max = x.min(), x.max()
        margin = (x_max - x_min) * 0.1 + 1000
        plt.xlim(x_min - margin, x_max + margin)

    plt.title(f'Audio Histogram - {title}')
    plt.xlabel('Sample Value')
    plt.ylabel('Count')
    plt.grid(True, alpha=0.3)

    
    # Save the plot
    output_file = f'{filename.replace(".txt", "")}_plot.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Plot saved as {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python plot_histogram.py <histogram_file.txt> <title>")
        sys.exit(1)
    
    plot_histogram(sys.argv[1], sys.argv[2])