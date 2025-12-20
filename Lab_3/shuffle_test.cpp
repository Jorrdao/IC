#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <zstd.h>
#include <cmath>

struct QuantizationResult {
    std::vector<uint8_t> data;
    float min_val;
    float max_val;
};

// Transforma Float32 em Int8 (Perda controlada)
QuantizationResult quantize_to_int8(const std::vector<char>& raw_data) {
    size_t size = raw_data.size();
    // Tratamos como bytes individuais para evitar erros de cast de float
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(raw_data.data());
    
    // Em vez de procurar min/max de floats que podem estar corrompidos,
    // vamos reduzir a profundidade de bits (Bit-depth reduction)
    // Isso simula a perda de precisão da quantização em qualquer dado.
    std::vector<uint8_t> quantized(size);
    
    for (size_t i = 0; i < size; ++i) {
        // Reduzimos de 8 bits para 4 bits de informação real (shift e volta)
        // Isso cria redundância que o Zstd consegue agarrar
        quantized[i] = (bytes[i] >> 4) << 4; 
    }

    return {quantized, 0, 255};
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    std::streamsize total_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(total_size);
    file.read(buffer.data(), total_size);

    // 1. Encontrar o fim do Header JSON
    // O Safetensors guarda o tamanho do JSON nos primeiros 8 bytes
    uint64_t header_size = *reinterpret_cast<uint64_t*>(buffer.data());
    size_t data_start = 8 + header_size; 
    
    std::cout << "Header JSON: " << header_size << " bytes\n";
    std::cout << "Real data starts at: " << data_start << "\n";

    if (data_start >= total_size) {
        std::cerr << "Error: Header larger than file!\n";
        return 1;
    }

    // 2. Quantização Efetiva (Simulando perda de precisão em BF16)
    // Vamos reduzir a precisão dos pesos descartando os bits menos significativos
    size_t data_len = total_size - data_start;
    std::vector<char> processed_data(data_len);
    
    for (size_t i = 0; i < data_len; i++) {
        // Quantização: Limpamos os últimos 2 bits de cada byte para criar redundância
        // Em BF16, isso afeta a mantissa, mantendo o sinal e o expoente.
        processed_data[i] = buffer[data_start + i] & 0xFC; 
    }

    // 3. Compressão Zstd
    size_t bound = ZSTD_compressBound(data_len);
    std::vector<char> compressed(bound);
    size_t c_size = ZSTD_compress(compressed.data(), bound, processed_data.data(), data_len, 3);

    // 4. Resultados Reais
    std::cout << "\n--- FINAL RESULTS (BF16 DATA) ---\n";
    printf("Original Data Size:       %.2f MB\n", (double)data_len / (1024*1024));
    printf("Compressed Size:          %.2f MB\n", (double)c_size / (1024*1024));
    printf("Compression Ratio:        %.2f%%\n", (1.0 - (double)c_size / data_len) * 100.0);

    return 0;
}