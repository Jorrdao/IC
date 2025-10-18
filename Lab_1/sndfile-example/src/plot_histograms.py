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
    plt.bar(x, y, width=1.0)
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