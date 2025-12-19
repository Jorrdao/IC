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

// Estrutura para guardar metadados de quantização
struct QuantMeta {
    float min_val;
    float max_val;
    float scale;
    float zero_point;
};

class MLCompressor {
private:
    static constexpr size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
    
    // Converter BF16 para float
    float bf16_to_float(uint16_t bf16) {
        uint32_t bits = static_cast<uint32_t>(bf16) << 16;
        float result;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    }
    
    // Converter float para BF16
    uint16_t float_to_bf16(float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(float));
        return static_cast<uint16_t>(bits >> 16);
    }
    
    // Quantizar float32 para int8
    int8_t quantize(float value, const QuantMeta& meta) {
        float scaled = (value - meta.zero_point) / meta.scale;
        return static_cast<int8_t>(std::clamp(scaled, -128.0f, 127.0f));
    }
    
    // Dequantizar int8 para float32
    float dequantize(int8_t qval, const QuantMeta& meta) {
        return qval * meta.scale + meta.zero_point;
    }
    
    // Calcular metadados de quantização
    QuantMeta compute_quant_meta(const std::vector<float>& values) {
        QuantMeta meta;
        meta.min_val = *std::min_element(values.begin(), values.end());
        meta.max_val = *std::max_element(values.begin(), values.end());
        meta.scale = (meta.max_val - meta.min_val) / 255.0f;
        meta.zero_point = meta.min_val;
        return meta;
    }
    
    // Delta encoding para int8
    void delta_encode(std::vector<int8_t>& data) {
        for (size_t i = data.size() - 1; i > 0; --i) {
            data[i] = data[i] - data[i - 1];
        }
    }
    
    // Delta decoding
    void delta_decode(std::vector<int8_t>& data) {
        for (size_t i = 1; i < data.size(); ++i) {
            data[i] = data[i] + data[i - 1];
        }
    }
    
public:
    bool compress(const std::string& input_file, const std::string& output_file) {
        auto start_total = std::chrono::high_resolution_clock::now();
        
        // Ler ficheiro
        std::ifstream in(input_file, std::ios::binary | std::ios::ate);
        if (!in) {
            std::cerr << "Erro ao abrir: " << input_file << std::endl;
            return false;
        }
        
        size_t file_size = in.tellg();
        in.seekg(0);
        
        // Ler header size (primeiros 8 bytes)
        uint64_t header_size;
        in.read(reinterpret_cast<char*>(&header_size), 8);
        
        std::cout << "Header JSON: " << header_size << " bytes" << std::endl;
        
        // Ler header (JSON)
        std::vector<char> header(header_size);
        in.read(header.data(), header_size);
        
        // Calcular tamanho dos dados
        size_t data_size = file_size - 8 - header_size;
        size_t num_bf16 = data_size / 2;
        
        std::cout << "Dados BF16: " << data_size << " bytes (" << num_bf16 << " valores)" << std::endl;
        std::cout << "\nFase 1: Converter BF16 -> Float32..." << std::endl;
        
        // Ler e converter BF16 -> Float32
        std::vector<uint16_t> bf16_data(num_bf16);
        in.read(reinterpret_cast<char*>(bf16_data.data()), data_size);
        in.close();
        
        std::vector<float> float_data(num_bf16);
        for (size_t i = 0; i < num_bf16; ++i) {
            float_data[i] = bf16_to_float(bf16_data[i]);
            if (i % (num_bf16 / 20) == 0) {
                std::cout << "." << std::flush;
            }
        }
        bf16_data.clear(); // Libertar memória
        
        std::cout << "\n\nFase 2: Quantizar Float32 -> INT8..." << std::endl;
        
        // Processar em blocos de 1M valores
        size_t block_size = 1024 * 1024;
        std::vector<QuantMeta> metas;
        std::vector<int8_t> quantized;
        quantized.reserve(num_bf16);
        
        for (size_t i = 0; i < num_bf16; i += block_size) {
            size_t current_block = std::min(block_size, num_bf16 - i);
            std::vector<float> block(float_data.begin() + i, 
                                     float_data.begin() + i + current_block);
            
            QuantMeta meta = compute_quant_meta(block);
            metas.push_back(meta);
            
            for (float val : block) {
                quantized.push_back(quantize(val, meta));
            }
            
            if ((i / block_size) % 10 == 0) {
                std::cout << "." << std::flush;
            }
        }
        float_data.clear();
        
        std::cout << "\n\nFase 3: Delta encoding..." << std::endl;
        delta_encode(quantized);
        
        std::cout << "Fase 4: Comprimir com Zstandard..." << std::endl;
        
        // Abrir output
        std::ofstream out(output_file, std::ios::binary);
        if (!out) {
            std::cerr << "Erro ao criar: " << output_file << std::endl;
            return false;
        }
        
        // Escrever formato: [header_size][header][num_blocks][metas][compressed_data]
        out.write(reinterpret_cast<char*>(&header_size), 8);
        out.write(header.data(), header_size);
        
        uint64_t num_blocks = metas.size();
        out.write(reinterpret_cast<char*>(&num_blocks), 8);
        out.write(reinterpret_cast<char*>(metas.data()), metas.size() * sizeof(QuantMeta));
        
        // Comprimir quantized data
        size_t compressed_bound = ZSTD_compressBound(quantized.size());
        std::vector<char> compressed(compressed_bound);
        
        size_t compressed_size = ZSTD_compress(
            compressed.data(), compressed.size(),
            quantized.data(), quantized.size(),
            15); // Nível moderado
        
        if (ZSTD_isError(compressed_size)) {
            std::cerr << "Erro ao comprimir: " << ZSTD_getErrorName(compressed_size) << std::endl;
            return false;
        }
        
        uint64_t comp_size = compressed_size;
        out.write(reinterpret_cast<char*>(&comp_size), 8);
        out.write(compressed.data(), compressed_size);
        
        out.close();
        
        auto end_total = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_total - start_total);
        
        size_t output_size = 8 + header_size + 8 + metas.size() * sizeof(QuantMeta) + 8 + compressed_size;
        double ratio = 100.0 * (1.0 - (double)output_size / file_size);
        
        std::cout << "\n\n=== RESULTADO ===" << std::endl;
        std::cout << "Original:        " << formatSize(file_size) << std::endl;
        std::cout << "Comprimido:      " << formatSize(output_size) << std::endl;
        std::cout << "  - Header:      " << formatSize(8 + header_size) << std::endl;
        std::cout << "  - Metadados:   " << formatSize(8 + metas.size() * sizeof(QuantMeta)) << std::endl;
        std::cout << "  - Dados:       " << formatSize(8 + compressed_size) << std::endl;
        std::cout << "Taxa:            " << std::fixed << std::setprecision(2) << ratio << "%" << std::endl;
        std::cout << "Tempo:           " << duration.count() << " s" << std::endl;
        std::cout << "\nNOTA: Compressão com perda (quantização INT8)" << std::endl;
        
        return true;
    }
    
    bool decompress(const std::string& input_file, const std::string& output_file) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::ifstream in(input_file, std::ios::binary);
        if (!in) {
            std::cerr << "Erro ao abrir: " << input_file << std::endl;
            return false;
        }
        
        // Ler header
        uint64_t header_size;
        in.read(reinterpret_cast<char*>(&header_size), 8);
        
        std::vector<char> header(header_size);
        in.read(header.data(), header_size);
        
        // Ler metadados
        uint64_t num_blocks;
        in.read(reinterpret_cast<char*>(&num_blocks), 8);
        
        std::vector<QuantMeta> metas(num_blocks);
        in.read(reinterpret_cast<char*>(metas.data()), num_blocks * sizeof(QuantMeta));
        
        // Ler dados comprimidos
        uint64_t compressed_size;
        in.read(reinterpret_cast<char*>(&compressed_size), 8);
        
        std::vector<char> compressed(compressed_size);
        in.read(compressed.data(), compressed_size);
        in.close();
        
        std::cout << "Descomprimindo..." << std::endl;
        
        // Descomprimir
        size_t decompressed_size = ZSTD_getFrameContentSize(compressed.data(), compressed_size);
        std::vector<int8_t> quantized(decompressed_size);
        
        size_t result = ZSTD_decompress(
            quantized.data(), quantized.size(),
            compressed.data(), compressed_size);
        
        if (ZSTD_isError(result)) {
            std::cerr << "Erro ao descomprimir: " << ZSTD_getErrorName(result) << std::endl;
            return false;
        }
        
        // Delta decode
        delta_decode(quantized);
        
        // Dequantizar
        std::vector<uint16_t> bf16_data;
        bf16_data.reserve(quantized.size());
        
        size_t block_size = 1024 * 1024;
        for (size_t i = 0; i < quantized.size(); i += block_size) {
            size_t current_block = std::min(block_size, quantized.size() - i);
            size_t meta_idx = i / block_size;
            const QuantMeta& meta = metas[meta_idx];
            
            for (size_t j = 0; j < current_block; ++j) {
                float val = dequantize(quantized[i + j], meta);
                bf16_data.push_back(float_to_bf16(val));
            }
        }
        
        // Escrever output
        std::ofstream out(output_file, std::ios::binary);
        out.write(reinterpret_cast<char*>(&header_size), 8);
        out.write(header.data(), header_size);
        out.write(reinterpret_cast<char*>(bf16_data.data()), bf16_data.size() * 2);
        out.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        
        std::cout << "Descompressão concluída em " << duration.count() << " s" << std::endl;
        
        return true;
    }
    
private:
    std::string formatSize(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;
        
        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return oss.str();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Compressor ML para Safetensors (com quantização INT8)" << std::endl;
        std::cout << "\nUso: " << argv[0] << " <c|d> <entrada> [saida]" << std::endl;
        std::cout << "\nModos:" << std::endl;
        std::cout << "  c - Comprimir (BF16 -> INT8 + Delta + Zstd)" << std::endl;
        std::cout << "  d - Descomprimir (restaurar BF16)" << std::endl;
        std::cout << "\nAVISO: Compressão com PERDA (quantização INT8)" << std::endl;
        std::cout << "       Precisão reduzida mas modelo ainda funcional" << std::endl;
        std::cout << "\nExemplos:" << std::endl;
        std::cout << "  " << argv[0] << " c model.safetensors model.mlc" << std::endl;
        std::cout << "  " << argv[0] << " d model.mlc model_restored.safetensors" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = (argc > 3) ? argv[3] : "";
    
    MLCompressor comp;
    
    if (mode == "c") {
        if (output.empty()) output = input + ".mlc";
        std::cout << "=== COMPRESSÃO ML ===" << std::endl;
        std::cout << "Input:  " << input << std::endl;
        std::cout << "Output: " << output << std::endl;
        std::cout << "\nEsta compressão usa quantização INT8 (com perda)" << std::endl;
        std::cout << "O modelo perde alguma precisão mas mantém funcionalidade\n" << std::endl;
        return !comp.compress(input, output);
        
    } else if (mode == "d") {
        if (output.empty()) output = "restored_" + input;
        std::cout << "Descompressão: " << input << " -> " << output << std::endl;
        return !comp.decompress(input, output);
        
    } else {
        std::cerr << "Modo inválido! Use 'c' ou 'd'" << std::endl;
        return 1;
    }
    
    return 0;
}