#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <zstd.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <omp.h>

struct QuantMeta {
    float min_val;
    float max_val;
    float scale;
    float zero_point;
};

enum CompressionMode : uint8_t {
    MODE_LOSSLESS_SMART = 1,
    MODE_LOSSY_INT8 = 2
};

class MLCompressor {
private:
    float bf16_to_float(uint16_t bf16) {
        uint32_t bits = static_cast<uint32_t>(bf16) << 16;
        float result;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    }

    uint16_t float_to_bf16(float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(float));
        return static_cast<uint16_t>(bits >> 16);
    }

    // --- NEW: PARALLEL SHUFFLE ---
    void byte_shuffle(std::vector<char>& data) {
        if (data.size() % 2 != 0) return;
        size_t half = data.size() / 2;
        std::vector<char> temp = data;
        
        // OpenMP Parallelization for Memory Movement
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < half; i++) {
            data[i] = temp[2 * i];            // Low Bytes
            data[half + i] = temp[2 * i + 1]; // High Bytes
        }
    }

    void byte_unshuffle(std::vector<char>& data) {
        if (data.size() % 2 != 0) return;
        size_t half = data.size() / 2;
        std::vector<char> temp = data;
        
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < half; i++) {
            data[2 * i] = temp[i];
            data[2 * i + 1] = temp[half + i];
        }
    }

    int8_t quantize(float value, const QuantMeta& meta) {
        // Clamp input to the meta range before scaling to avoid wrapping
        float clamped = std::clamp(value, meta.min_val, meta.max_val);
        float scaled = (clamped - meta.zero_point) / meta.scale;
        return static_cast<int8_t>(std::clamp(scaled, -128.0f, 127.0f));
    }

    float dequantize(int8_t qval, const QuantMeta& meta) {
        return qval * meta.scale + meta.zero_point;
    }

    // --- IMPROVED: SMART RANGE SELECTION ---
    QuantMeta compute_quant_meta(const std::vector<float>& values) {
        QuantMeta meta;
        
        // Calculate Mean and StdDev to detect outliers
        double sum = 0, sq_sum = 0;
        for(float v : values) { sum += v; sq_sum += v*v; }
        double mean = sum / values.size();
        double variance = (sq_sum / values.size()) - (mean * mean);
        double stddev = std::sqrt(std::max(0.0, variance));

        // Clip range to Mean +/- 3 Sigma (covers 99.7% of data)
        // This ignores extreme outliers that ruin quantization resolution
        float sigma_limit = 3.5f; // Adjustable
        meta.min_val = static_cast<float>(mean - sigma_limit * stddev);
        meta.max_val = static_cast<float>(mean + sigma_limit * stddev);
        
        // Fallback: If range is too small (constant block), use real min/max
        auto minmax = std::minmax_element(values.begin(), values.end());
        if (meta.min_val > *minmax.first) meta.min_val = *minmax.first;
        if (meta.max_val < *minmax.second) meta.max_val = *minmax.second;

        if (meta.max_val == meta.min_val) meta.max_val += 1e-6f;
        
        meta.scale = (meta.max_val - meta.min_val) / 255.0f;
        meta.zero_point = meta.min_val;
        return meta;
    }

    void delta_encode(std::vector<int8_t>& data) {
        for (size_t i = data.size() - 1; i > 0; --i) data[i] -= data[i - 1];
    }

    void delta_decode(std::vector<int8_t>& data) {
        for (size_t i = 1; i < data.size(); ++i) data[i] += data[i - 1];
    }

public:
    bool compress(const std::string& input_file, const std::string& output_file, CompressionMode mode, float prune_threshold = 0.0f) {
        auto start_total = std::chrono::high_resolution_clock::now();

        std::ifstream in(input_file, std::ios::binary | std::ios::ate);
        if (!in) { std::cerr << " Erro: Input file.\n"; return false; }
        size_t file_size = in.tellg();
        in.seekg(0);

        uint64_t header_size;
        in.read(reinterpret_cast<char*>(&header_size), 8);
        std::vector<char> header(header_size);
        in.read(header.data(), header_size);
        size_t data_size = file_size - 8 - header_size;
        
        std::cout << " Data: " << formatSize(data_size) << "\n";
        
        std::ofstream out(output_file, std::ios::binary);
        out.write(reinterpret_cast<char*>(&header_size), 8);
        out.write(header.data(), header_size);
        out.put(static_cast<char>(mode));

        std::vector<char> compressed_payload;

        if (mode == MODE_LOSSLESS_SMART) {
            std::cout << " Mode: SMART LOSSLESS (Parallel Shuffle)\n";
            std::vector<char> raw_data(data_size);
            in.read(raw_data.data(), data_size);
            
            byte_shuffle(raw_data); // Now Multithreaded!
            
            size_t bound = ZSTD_compressBound(data_size);
            compressed_payload.resize(bound);
            size_t csize = ZSTD_compress(compressed_payload.data(), bound, raw_data.data(), data_size, 10);
            compressed_payload.resize(csize);

        } else if (mode == MODE_LOSSY_INT8) {
            std::cout << " Mode: LOSSY INT8 (Sigma Clipping + OpenMP)\n";
            size_t num_bf16 = data_size / 2;
            std::vector<uint16_t> bf16_data(num_bf16);
            in.read(reinterpret_cast<char*>(bf16_data.data()), data_size);

            std::vector<int8_t> quantized(num_bf16);
            size_t block_size = 1024 * 1024;
            size_t num_blocks = (num_bf16 + block_size - 1) / block_size;
            std::vector<QuantMeta> metas(num_blocks);

            #pragma omp parallel for schedule(dynamic)
            for (size_t k = 0; k < num_blocks; ++k) {
                size_t start_idx = k * block_size;
                size_t end_idx = std::min(start_idx + block_size, num_bf16);
                size_t len = end_idx - start_idx;
                std::vector<float> fblock; fblock.reserve(len);
                
                for(size_t j=0; j<len; ++j) {
                    float val = bf16_to_float(bf16_data[start_idx+j]);
                    if (std::abs(val) < prune_threshold) val = 0.0f;
                    fblock.push_back(val);
                }

                QuantMeta meta = compute_quant_meta(fblock); // Uses new Sigma Clipping
                metas[k] = meta;

                for (size_t j=0; j<len; ++j) {
                    quantized[start_idx+j] = quantize(fblock[j], meta);
                }
            }

            delta_encode(quantized);

            // Write Metas
            uint64_t nb = metas.size();
            out.write((char*)&nb, 8);
            out.write((char*)metas.data(), metas.size() * sizeof(QuantMeta));

            // Compress
            size_t bound = ZSTD_compressBound(quantized.size());
            compressed_payload.resize(bound);
            size_t csize = ZSTD_compress(compressed_payload.data(), bound, quantized.data(), quantized.size(), 19);
            compressed_payload.resize(csize);
        }

        uint64_t c_size = compressed_payload.size();
        out.write((char*)&c_size, 8);
        out.write(compressed_payload.data(), c_size);

        auto end = std::chrono::high_resolution_clock::now();
        size_t final_size = out.tellp();
        double ratio = (1.0 - (double)final_size / file_size) * 100.0;
        
        std::cout << "Done. Ratio: " << std::fixed << std::setprecision(2) << ratio << "% Time: " 
                  << std::chrono::duration<double>(end - start_total).count() << "s\n";
        return true;
    }

    bool decompress(const std::string& input_file, const std::string& output_file) {
        std::ifstream in(input_file, std::ios::binary);
        if (!in) return false;

        uint64_t header_size;
        in.read((char*)&header_size, 8);
        std::vector<char> header(header_size);
        in.read(header.data(), header_size);

        char mode_byte; in.get(mode_byte);
        CompressionMode mode = static_cast<CompressionMode>(mode_byte);

        std::vector<char> final_data;

        if (mode == MODE_LOSSLESS_SMART) {
            uint64_t c_size; in.read((char*)&c_size, 8);
            std::vector<char> compressed(c_size);
            in.read(compressed.data(), c_size);
            unsigned long long orig_size = ZSTD_getFrameContentSize(compressed.data(), c_size);
            final_data.resize(orig_size);
            ZSTD_decompress(final_data.data(), orig_size, compressed.data(), c_size);
            byte_unshuffle(final_data); // Parallel Unshuffle
        } 
        else if (mode == MODE_LOSSY_INT8) {
            uint64_t num_blocks; in.read((char*)&num_blocks, 8);
            std::vector<QuantMeta> metas(num_blocks);
            in.read((char*)metas.data(), num_blocks * sizeof(QuantMeta));

            uint64_t c_size; in.read((char*)&c_size, 8);
            std::vector<char> compressed(c_size);
            in.read(compressed.data(), c_size);

            size_t num_elements = ZSTD_getFrameContentSize(compressed.data(), c_size);
            std::vector<int8_t> quantized(num_elements);
            ZSTD_decompress(quantized.data(), num_elements, compressed.data(), c_size);
            delta_decode(quantized);

            final_data.resize(num_elements * 2);
            uint16_t* bf16_ptr = reinterpret_cast<uint16_t*>(final_data.data());
            size_t block_size = 1024 * 1024;

            #pragma omp parallel for schedule(dynamic)
            for (size_t i = 0; i < num_elements; ++i) {
                float val = dequantize(quantized[i], metas[std::min(i/block_size, metas.size()-1)]);
                bf16_ptr[i] = float_to_bf16(val);
            }
        }

        std::ofstream out(output_file, std::ios::binary);
        out.write((char*)&header_size, 8);
        out.write(header.data(), header_size);
        out.write(final_data.data(), final_data.size());
        return true;
    }

    void calculate_error(const std::string& original, const std::string& restored) {
        std::ifstream f1(original, std::ios::binary);
        std::ifstream f2(restored, std::ios::binary);
        
        // Skip headers
        uint64_t h1; f1.read((char*)&h1, 8); f1.seekg(8+h1);
        uint64_t h2; f2.read((char*)&h2, 8); f2.seekg(8+h2);

        double total_sq_error = 0;
        size_t count = 0;
        std::vector<uint16_t> buf1(1024*1024), buf2(1024*1024);

        while(f1 && f2) {
            f1.read((char*)buf1.data(), buf1.size()*2);
            f2.read((char*)buf2.data(), buf2.size()*2);
            size_t n = f1.gcount()/2;
            if(n==0) break;
            
            for(size_t i=0; i<n; i++) {
                float v1 = bf16_to_float(buf1[i]);
                float v2 = bf16_to_float(buf2[i]);
                double diff = v1 - v2;
                total_sq_error += diff * diff;
                count++;
            }
        }
        double mse = total_sq_error / count;
        std::cout << "\n MSE: " << mse << " (Lower is better)\n";
    }

    std::string formatSize(size_t bytes) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0*1024.0)) << " MB";
        return oss.str();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    MLCompressor comp;
    std::string mode = argv[1];
    
    if (mode == "l") comp.compress(argv[2], (argc>3?argv[3]:"out.mlc"), MODE_LOSSLESS_SMART);
    else if (mode == "q") comp.compress(argv[2], (argc>3?argv[3]:"out.mlc"), MODE_LOSSY_INT8, (argc>4?std::stof(argv[4]):0.0f));
    else if (mode == "d") comp.decompress(argv[2], (argc>3?argv[3]:"restored.safetensors"));
    else if (mode == "v") comp.calculate_error(argv[2], argv[3]);
    return 0;
}