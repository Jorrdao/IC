# Compiler Variables
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -fopenmp
LDFLAGS_COMMON = -lzstd
LDFLAGS_MULTI = -lzstd -llzma -lz -lbz2

# Input File
INPUT_FILE = model.safetensors

# Output Files (Intermediate)
OUTPUT_LOSSLESS_C = model_lossless.mlc
OUTPUT_LOSSLESS_D = model_restored_l.safetensors
OUTPUT_LOSSY_C = model_lossy.mlc
OUTPUT_LOSSY_D = model_restored_q.safetensors

# Executable List
TARGETS = analyze multi_compress compressor int8 shuffle_test

# --- Main Rules ---

.PHONY: all clean run download help

all: $(TARGETS)
	@echo " Compilation successful!"

# Individual Compilation Rules

analyze: analyze.cpp
	@echo " Compiling analyze..."
	$(CXX) $(CXXFLAGS) analyze.cpp -o analyze

multi_compress: multi_compress.cpp
	@echo " Compiling multi_compress..."
	$(CXX) $(CXXFLAGS) multi_compress.cpp -o multi_compress $(LDFLAGS_MULTI)

compressor: compressor.cpp
	@echo " Compiling compressor (MLCompressor)..."
	$(CXX) $(CXXFLAGS) compressor.cpp -o compressor $(LDFLAGS_COMMON)

int8: int8.cpp
	@echo " Compiling int8 test..."
	$(CXX) $(CXXFLAGS) int8.cpp -o int8 $(LDFLAGS_COMMON)

shuffle_test: shuffle_test.cpp
	@echo " Compiling shuffle_test..."
	$(CXX) $(CXXFLAGS) shuffle_test.cpp -o shuffle_test $(LDFLAGS_COMMON)

# --- Execution Rules ---

run: all
	@if [ ! -f $(INPUT_FILE) ]; then \
		echo "Error: $(INPUT_FILE) not found!"; \
		echo "   Run 'make download' or add the file to the folder."; \
		exit 1; \
	fi

	@echo "\n========================================"
	@echo " 1. RUNNING: ANALYZE"
	@echo "========================================"
	./analyze $(INPUT_FILE)
	
	@echo "\n========================================"
	@echo " 2. RUNNING: BENCHMARKS"
	@echo "========================================"
	./multi_compress $(INPUT_FILE)
	
	@echo "\n========================================"
	@echo "3. SOLUTION: SMART LOSSLESS (Byte Shuffle)"
	@echo "========================================"
	@echo "--- Compressing ---"
	@/usr/bin/time -v ./compressor l $(INPUT_FILE) $(OUTPUT_LOSSLESS_C) 2>&1 | grep -E "User time|System time|Percent of CPU|Maximum resident set size"
	
	@echo "--- Decompressing ---"
	@/usr/bin/time -v ./compressor d $(OUTPUT_LOSSLESS_C) $(OUTPUT_LOSSLESS_D) 2>&1 | grep -E "User time|System time|Percent of CPU|Maximum resident set size"
	
	@echo "--- Integrity ---"
	./compressor v $(INPUT_FILE) $(OUTPUT_LOSSLESS_D)

	@echo "\n========================================"
	@echo "⚡ 4. SOLUTION: LOSSY INT8 (OpenMP + Quantization)"
	@echo "========================================"
	@echo "--- Compressing ---"
	@/usr/bin/time -v ./compressor q $(INPUT_FILE) $(OUTPUT_LOSSY_C) 2>&1 | grep -E "User time|System time|Percent of CPU|Maximum resident set size"
	
	@echo "--- Decompressing ---"
	@/usr/bin/time -v ./compressor d $(OUTPUT_LOSSY_C) $(OUTPUT_LOSSY_D) 2>&1 | grep -E "User time|System time|Percent of CPU|Maximum resident set size"
	
	@echo "--- Quality Check ---"
	./compressor v $(INPUT_FILE) $(OUTPUT_LOSSY_D)

	@echo "\n Full Test Cycle Complete."

# --- Utilities ---

download:
	@echo "Downloading model Qwen2-0.5B..."
	wget -nc https://huggingface.co/Qwen/Qwen2-0.5B/resolve/main/model.safetensors

clean:
	@echo "🧹 Cleaning files..."
	rm -f $(TARGETS) 
	rm -f $(OUTPUT_LOSSLESS_C) $(OUTPUT_LOSSLESS_D)
	rm -f $(OUTPUT_LOSSY_C) $(OUTPUT_LOSSY_D)
	rm -f *.o

help:
	@echo "Available Commands:"
	@echo "  make          - Compile all programs"
	@echo "  make run      - Compile and run the full test suite (Analyze -> Benchmark -> Lossless -> Lossy)"
	@echo "  make download - Download the model.safetensors file"
	@echo "  make clean    - Remove executables and temporary files"