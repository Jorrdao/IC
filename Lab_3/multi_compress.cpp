#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <zstd.h>
#include <lzma.h>
#include <zlib.h>
#include <bzlib.h>
#include <algorithm>

// Compilar: g++ -O3 -std=c++17 multi_compress.cpp -o multi_compress -lzstd -llzma -lz -lbz2

struct Result {
    std::string name;
    double compress_time;
    double decompress_time;
    size_t compressed_size;
    double ratio;
};

std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> data(size);
    file.read(data.data(), size);
    return data;
}

// Zstd compression
Result test_zstd(const std::vector<char>& data, int level) {
    auto start = std::chrono::high_resolution_clock::now();
    
    size_t bound = ZSTD_compressBound(data.size());
    std::vector<char> compressed(bound);
    
    size_t comp_size = ZSTD_compress(
        compressed.data(), bound,
        data.data(), data.size(),
        level
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    double comp_time = std::chrono::duration<double>(end - start).count();
    
    // Decompress test
    start = std::chrono::high_resolution_clock::now();
    unsigned long long orig_size = ZSTD_getFrameContentSize(compressed.data(), comp_size);
    std::vector<char> decompressed(orig_size);
    ZSTD_decompress(decompressed.data(), orig_size, compressed.data(), comp_size);
    end = std::chrono::high_resolution_clock::now();
    double decomp_time = std::chrono::duration<double>(end - start).count();
    
    double ratio = (1.0 - (double)comp_size / data.size()) * 100.0;
    
    return {"Zstd-" + std::to_string(level), comp_time, decomp_time, comp_size, ratio};
}

// LZMA compression
Result test_lzma(const std::vector<char>& data, int level) {
    auto start = std::chrono::high_resolution_clock::now();
    
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);
    
    std::vector<char> compressed;
    compressed.reserve(data.size());
    
    strm.next_in = (const uint8_t*)data.data();
    strm.avail_in = data.size();
    
    std::vector<char> buffer(1024 * 1024);
    
    do {
        strm.next_out = (uint8_t*)buffer.data();
        strm.avail_out = buffer.size();
        
        lzma_code(&strm, LZMA_FINISH);
        
        size_t produced = buffer.size() - strm.avail_out;
        compressed.insert(compressed.end(), buffer.begin(), buffer.begin() + produced);
    } while (strm.avail_out == 0);
    
    lzma_end(&strm);
    
    auto end = std::chrono::high_resolution_clock::now();
    double comp_time = std::chrono::duration<double>(end - start).count();
    
    // Decompress test
    start = std::chrono::high_resolution_clock::now();
    lzma_stream strm2 = LZMA_STREAM_INIT;
    lzma_stream_decoder(&strm2, UINT64_MAX, 0);
    
    std::vector<char> decompressed;
    strm2.next_in = (const uint8_t*)compressed.data();
    strm2.avail_in = compressed.size();
    
    do {
        strm2.next_out = (uint8_t*)buffer.data();
        strm2.avail_out = buffer.size();
        lzma_code(&strm2, LZMA_FINISH);
        size_t produced = buffer.size() - strm2.avail_out;
        decompressed.insert(decompressed.end(), buffer.begin(), buffer.begin() + produced);
    } while (strm2.avail_out == 0);
    
    lzma_end(&strm2);
    end = std::chrono::high_resolution_clock::now();
    double decomp_time = std::chrono::duration<double>(end - start).count();
    
    double ratio = (1.0 - (double)compressed.size() / data.size()) * 100.0;
    
    return {"LZMA-" + std::to_string(level), comp_time, decomp_time, compressed.size(), ratio};
}

// Gzip compression
Result test_gzip(const std::vector<char>& data, int level) {
    auto start = std::chrono::high_resolution_clock::now();
    
    uLong bound = compressBound(data.size());
    std::vector<Bytef> compressed(bound);
    
    compress2(compressed.data(), &bound, (const Bytef*)data.data(), data.size(), level);
    compressed.resize(bound);
    
    auto end = std::chrono::high_resolution_clock::now();
    double comp_time = std::chrono::duration<double>(end - start).count();
    
    // Decompress
    start = std::chrono::high_resolution_clock::now();
    std::vector<Bytef> decompressed(data.size());
    uLong decomp_size = data.size();
    uncompress(decompressed.data(), &decomp_size, compressed.data(), compressed.size());
    end = std::chrono::high_resolution_clock::now();
    double decomp_time = std::chrono::duration<double>(end - start).count();
    
    double ratio = (1.0 - (double)compressed.size() / data.size()) * 100.0;
    
    return {"Gzip-" + std::to_string(level), comp_time, decomp_time, compressed.size(), ratio};
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Uso: ./multi_compress <ficheiro.safetensors>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "🧪 TESTE DE MÚLTIPLOS COMPRESSORES\n";
    std::cout << std::string(80, '=') << "\n\n";
    
    // Ler ficheiro
    std::cout << "📖 A ler ficheiro...\n";
    auto data = read_file(filename);
    size_t original_size = data.size();
    
    std::cout << "📦 Tamanho: " << original_size / (1024.0 * 1024 * 1024) << " GB\n\n";
    
    std::vector<Result> results;
    
    // Testar Zstd
    std::cout << "⏳ Testando Zstd...\n";
    results.push_back(test_zstd(data, 1));
    std::cout << "   Level 1: " << results.back().ratio << "%\n";
    results.push_back(test_zstd(data, 10));
    std::cout << "   Level 10: " << results.back().ratio << "%\n";
    results.push_back(test_zstd(data, 19));
    std::cout << "   Level 19: " << results.back().ratio << "%\n";
    results.push_back(test_zstd(data, 22));
    std::cout << "   Level 22: " << results.back().ratio << "%\n";
    
    // Testar LZMA
    std::cout << "\n⏳ Testando LZMA...\n";
    results.push_back(test_lzma(data, 6));
    std::cout << "   Level 6: " << results.back().ratio << "%\n";
    results.push_back(test_lzma(data, 9));
    std::cout << "   Level 9: " << results.back().ratio << "%\n";
    
    // Testar Gzip
    std::cout << "\n⏳ Testando Gzip...\n";
    results.push_back(test_gzip(data, 6));
    std::cout << "   Level 6: " << results.back().ratio << "%\n";
    results.push_back(test_gzip(data, 9));
    std::cout << "   Level 9: " << results.back().ratio << "%\n";
    
    // Relatório
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "📊 RESULTADOS COMPARATIVOS\n";
    std::cout << std::string(80, '=') << "\n\n";
    
    std::cout << "Compressor     Ratio      Size(GB)   Comp(s)    Decomp(s)  MB/s\n";
    std::cout << std::string(80, '-') << "\n";
    
    for (const auto& r : results) {
        double speed = (original_size / (1024.0 * 1024)) / r.compress_time;
        printf("%-14s %6.2f%%   %8.3f   %8.2f   %9.2f   %6.1f\n",
               r.name.c_str(), r.ratio,
               r.compressed_size / (1024.0 * 1024 * 1024),
               r.compress_time, r.decompress_time, speed);
    }
    
    // Melhor de cada categoria
    auto best_ratio = *std::max_element(results.begin(), results.end(),
        [](const Result& a, const Result& b) { return a.ratio < b.ratio; });
    
    auto fastest = *std::min_element(results.begin(), results.end(),
        [](const Result& a, const Result& b) { return a.compress_time < b.compress_time; });
    
    std::cout << "\n🏆 Melhor compressão: " << best_ratio.name << " → " << best_ratio.ratio << "%\n";
    std::cout << "⚡ Mais rápido: " << fastest.name << " → " << fastest.compress_time << "s\n";
    
    return 0;
}