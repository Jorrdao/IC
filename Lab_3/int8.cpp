#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <zstd.h>

struct QuantizationResult {
    std::vector<uint8_t> data;
    float min_val;
    float max_val;
};

// Transforma Float32 em Int8 (Perda controlada)
QuantizationResult quantize_to_int8(const std::vector<char>& raw_data) {
    size_t num_elements = raw_data.size() / sizeof(float);
    const float* floats = reinterpret_cast<const float*>(raw_data.data());
    
    float min_val = floats[0];
    float max_val = floats[0];
    for (size_t i = 0; i < num_elements; ++i) {
        if (floats[i] < min_val) min_val = floats[i];
        if (floats[i] > max_val) max_val = floats[i];
    }

    std::vector<uint8_t> quantized(num_elements);
    float range = max_val - min_val;
    
    for (size_t i = 0; i < num_elements; ++i) {
        // Mapeia o float para o intervalo [0, 255]
        quantized[i] = static_cast<uint8_t>((floats[i] - min_val) / range * 255.0f);
    }

    return {quantized, min_val, max_val};
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    std::ifstream file(argv[1], std::ios::binary);
    std::vector<char> original((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    auto start = std::chrono::high_resolution_clock::now();

    // 1. Quantização (Reduz logo 75% do tamanho bruto)
    auto q_res = quantize_to_int8(original);
    
    // 2. Compressão do resultado quantizado (Zstd rápido)
    size_t bound = ZSTD_compressBound(q_res.data.size());
    std::vector<char> compressed(bound);
    size_t c_size = ZSTD_compress(compressed.data(), bound, q_res.data.data(), q_res.data.size(), 3);

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    std::cout << "--- Results: ---\n";
    std::cout << "Original Size:   " << original.size() / (1024*1024) << " MB\n";
    std::cout << "Compressed Size: " << c_size / (1024*1024) << " MB\n";
    std::cout << "Final Ratio:        " << (1.0 - (double)c_size / original.size()) * 100.0 << "%\n";
    std::cout << "Total Time:        " << duration << "s\n";
    std::cout << "Speed:         " << (original.size() / 1024.0 / 1024.0) / duration << " MB/s\n";

    return 0;
}