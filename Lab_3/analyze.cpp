#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <iomanip>

// Compilar: g++ -O3 -std=c++17 analyze.cpp -o analyze

struct TensorInfo {
    std::string name;
    std::string dtype;
    std::vector<size_t> shape;
    size_t offset_start;
    size_t offset_end;
    size_t size_bytes;
};

double calculate_entropy(const std::vector<uint8_t>& data, size_t sample_size) {
    // Calcular entropia de Shannon em bits/byte
    std::map<uint8_t, size_t> freq;
    
    size_t to_analyze = std::min(sample_size, data.size());
    
    for (size_t i = 0; i < to_analyze; i++) {
        freq[data[i]]++;
    }
    
    double entropy = 0.0;
    for (const auto& pair : freq) {
        double p = static_cast<double>(pair.second) / to_analyze;
        if (p > 0) {
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

void analyze_data_patterns(const std::vector<uint8_t>& data, size_t sample_size) {
    std::cout << "\n Pattern analyzis:\n";
    std::cout << std::string(70, '-') << "\n";
    
    size_t to_analyze = std::min(sample_size, data.size());
    
    // Contar zeros
    size_t zero_count = 0;
    for (size_t i = 0; i < to_analyze; i++) {
        if (data[i] == 0) zero_count++;
    }
    
    double zero_ratio = (zero_count * 100.0) / to_analyze;
    std::cout << " Zeros: " << zero_ratio << "%\n";
    
    // Valores únicos
    std::map<uint8_t, size_t> unique_values;
    for (size_t i = 0; i < to_analyze; i++) {
        unique_values[data[i]]++;
    }
    
    std::cout << " Unique vlues (in " << to_analyze << " bytes): " 
              << unique_values.size() << "/256\n";
    
    // Entropia
    double entropy = calculate_entropy(data, sample_size);
    std::cout << "Entropy: " << std::fixed << std::setprecision(4) 
              << entropy << " bits/byte (max=8.0)\n";
    
    double theoretical_compression = entropy / 8.0 * 100.0;
    std::cout << " Theoretical minimal compression: " << theoretical_compression << "%\n";
    
    // Repetições consecutivas
    size_t consecutive_same = 0;
    for (size_t i = 1; i < to_analyze; i++) {
        if (data[i] == data[i-1]) consecutive_same++;
    }
    
    double repeat_ratio = (consecutive_same * 100.0) / to_analyze;
    std::cout << "Consecutive repeats: " << repeat_ratio << "%\n";
    
    // Top 10 valores mais frequentes
    std::vector<std::pair<size_t, uint8_t>> freq_vec;
    for (const auto& pair : unique_values) {
        freq_vec.push_back({pair.second, pair.first});
    }
    std::sort(freq_vec.rbegin(), freq_vec.rend());
    
    std::cout << "\n Top 10 most frequent values:\n";
    for (size_t i = 0; i < std::min(size_t(10), freq_vec.size()); i++) {
        double percent = (freq_vec[i].first * 100.0) / to_analyze;
        std::cout << "   " << (int)freq_vec[i].second << ": " 
                  << freq_vec[i].first << " times (" << percent << "%)\n";
    }
}

void analyze_float_distribution(const std::vector<uint8_t>& data, size_t sample_size) {
    // Analisar como floats
    if (data.size() < 4) return;
    
    std::cout << "\n FLOAT ANALYSIS (F32):\n";
    std::cout << std::string(70, '-') << "\n";
    
    size_t num_floats = std::min(sample_size / 4, data.size() / 4);
    const float* floats = reinterpret_cast<const float*>(data.data());
    
    float min_val = floats[0];
    float max_val = floats[0];
    double sum = 0;
    size_t zero_floats = 0;
    size_t small_floats = 0; // |x| < 0.01
    
    for (size_t i = 0; i < num_floats; i++) {
        float val = floats[i];
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
        sum += val;
        
        if (val == 0.0f) zero_floats++;
        if (std::abs(val) < 0.01f) small_floats++;
    }
    
    double mean = sum / num_floats;
    
    std::cout << " Min: " << min_val << "\n";
    std::cout << " Max: " << max_val << "\n";
    std::cout << " Mean: " << mean << "\n";
    std::cout << " Zeros: " << (zero_floats * 100.0 / num_floats) << "%\n";
    std::cout << " Small (|x|<0.01): " << (small_floats * 100.0 / num_floats) << "%\n";
    
    // Potencial de quantização
    float range = max_val - min_val;
    std::cout << "\n Quantization Potential:\n";
    std::cout << "   Range: " << range << "\n";
    
    // F32 → F16
    std::cout << "   F32→F16: 50% reduction guaranteed + compression\n";
    
    // F32 → INT8
    float int8_quantization_error = range / 255.0;
    std::cout << "   F32→INT8: 75% reduction, quantization error ≈ " 
              << int8_quantization_error << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./analyze <file.safetensors>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    std::ifstream file(filename, std::ios::binary);
    
    if (!file) {
        std::cerr << " Error opening the file: " << filename << "\n";
        return 1;
    }
    
    // Obter tamanho do ficheiro
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " DETAILED FILE ANALYSIS\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << " File: " << filename << "\n";
    std::cout << " Size: " << file_size / (1024.0 * 1024 * 1024) << " GB\n";
    
    // Read header
    uint64_t header_size;
    file.read(reinterpret_cast<char*>(&header_size), 8);
    
    std::vector<char> header(header_size);
    file.read(header.data(), header_size);
    
    std::cout << "Header: " << header_size / 1024.0 << " KB\n";
    
    // Read data
    size_t data_size = file_size - 8 - header_size;
    std::cout << " Data: " << data_size / (1024.0 * 1024 * 1024) << " GB\n";
    
    // Ler amostra dos dados (primeiros 100MB ou menos)
    size_t sample_size = std::min(size_t(100 * 1024 * 1024), data_size);
    std::vector<uint8_t> sample(sample_size);
    file.read(reinterpret_cast<char*>(sample.data()), sample_size);
    
    std::cout << " Analyzing sample of " << sample_size / (1024.0 * 1024) << " MB...\n";
    
    // Análises
    analyze_data_patterns(sample, sample_size);
    analyze_float_distribution(sample, sample_size);
    
    // Testar compressibilidade real
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " Suggestions\n";
    std::cout << std::string(70, '=') << "\n";
    
    double entropy = calculate_entropy(sample, sample_size);
    
    if (entropy > 7.5) {
        std::cout << "  Data is highly random (entropy > 7.5)\n";
        std::cout << "   - Is it already compressed or encrypted?\n";
        std::cout << "   - Compression will be limited (~10-20%)\n";
    } else if (entropy > 6.5) {
        std::cout << "  Data is little compressible (entropy 6.5-7.5)\n";
        std::cout << "   - Expected compression: 20-35%\n";
        std::cout << "   - Recommendation: Use quantization\n";
    } else if (entropy > 5.0) {
        std::cout << " Data is reasonably compressible (entropy 5-6.5)\n";
        std::cout << "   - Expected compression: 35-50%\n";
        std::cout << "   - Use Zstd level 19-22\n";
    } else {
        std::cout << " Data is very compressible (entropy < 5)\n";
        std::cout << "   - Expected compression: 50%+\n";
        std::cout << "   - Any compressor will work well\n";
    }

    
    return 0;
}