#include "AudioCodec.h"
#include "golomb.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>

void printUsage(const char* progname) {
    std::cout << "╔════════════════════════════════════════════════════╗\n";
    std::cout << "║  Codec de Áudio Lossless - Codificação Golomb     ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Uso:\n";
    std::cout << "  Compressão:\n";
    std::cout << "    " << progname << " -c <input.wav> <output.glmb> [opções]\n\n";
    std::cout << "  Descompressão:\n";
    std::cout << "    " << progname << " -d <input.glmb> <output.wav>\n\n";
    
    std::cout << "Opções de compressão:\n";
    std::cout << "  -m <valor>        Parâmetro m fixo do Golomb (padrão: adaptativo)\n";
    std::cout << "                    Use 0 para modo adaptativo (recomendado)\n";
    std::cout << "  -p <0|1|2|3>      Preditor (padrão: 1)\n";
    std::cout << "                    0 = nenhum preditor\n";
    std::cout << "                    1 = ordem 1 (x[n-1])  recomendado\n";
    std::cout << "                    2 = ordem 2 (2*x[n-1] - x[n-2])\n";
    std::cout << "                    3 = ordem 3\n";
    std::cout << "  -s <0|1>          Modo de sinal (padrão: 1)\n";
    std::cout << "                    0 = sign-magnitude\n";
    std::cout << "                    1 = interleaving  recomendado\n";
    std::cout << "  -ms <0|1>         Mid-Side para estéreo (padrão: 1)\n";
    std::cout << "                    0 = não usar\n";
    std::cout << "                    1 = usar  recomendado para estéreo\n";
    
    std::cout << "\nExemplos:\n";
    std::cout << "  # Compressão padrão (adaptativo, melhor qualidade)\n";
    std::cout << "  " << progname << " -c audio.wav audio.glmb\n\n";
    
    std::cout << "  # Compressão com m fixo e preditor ordem 2\n";
    std::cout << "  " << progname << " -c audio.wav audio.glmb -m 16 -p 2\n\n";
    
    std::cout << "  # Compressão sem Mid-Side\n";
    std::cout << "  " << progname << " -c audio.wav audio.glmb -ms 0\n\n";
    
    std::cout << "  # Descompressão\n";
    std::cout << "  " << progname << " -d audio.glmb decoded.wav\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string mode = argv[1];
    std::string input_file = argv[2];
    std::string output_file = argv[3];
    
    if (mode == "-c") {
        // ==================== COMPRESSÃO ====================
        
        // Valores padrão
        int m = 0;  // 0 = adaptativo
        AudioCodec::PredictorType pred = AudioCodec::PredictorType::ORDER_1;
        GolombCoder::SignMode sign_mode = GolombCoder::SignMode::INTERLEAVING;
        bool mid_side = true;
        
        // Parse opções
        for (int i = 4; i < argc; i += 2) {
            if (i + 1 >= argc) {
                std::cerr << "Erro: Opção " << argv[i] << " requer um valor\n";
                return 1;
            }
            
            std::string opt = argv[i];
            std::string val = argv[i + 1];
            
            try {
                if (opt == "-m") {
                    m = std::stoi(val);
                    if (m < 0 || m > 1024) {
                        std::cerr << "Erro: m deve estar entre 0 e 1024\n";
                        return 1;
                    }
                } else if (opt == "-p") {
                    int p = std::stoi(val);
                    if (p < 0 || p > 3) {
                        std::cerr << "Erro: preditor deve ser 0, 1, 2 ou 3\n";
                        return 1;
                    }
                    pred = static_cast<AudioCodec::PredictorType>(p);
                } else if (opt == "-s") {
                    int s = std::stoi(val);
                    if (s != 0 && s != 1) {
                        std::cerr << "Erro: modo de sinal deve ser 0 ou 1\n";
                        return 1;
                    }
                    sign_mode = s == 0 ? SignMode::SIGN_MAGNITUDE : SignMode::INTERLEAVING;
                } else if (opt == "-ms") {
                    int ms = std::stoi(val);
                    if (ms != 0 && ms != 1) {
                        std::cerr << "Erro: mid-side deve ser 0 ou 1\n";
                        return 1;
                    }
                    mid_side = (ms == 1);
                } else {
                    std::cerr << "Erro: Opção desconhecida: " << opt << "\n";
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Erro ao processar opção " << opt << ": " << e.what() << "\n";
                return 1;
            }
        }
        
        bool adaptive = (m == 0);
        
        // Criar codec com configurações
        AudioCodec codec(m, sign_mode, pred, adaptive, mid_side);
        AudioCodec::CompressionStats stats;
        
        std::cout << "\n╔════════════════════════════════════════════════════╗\n";
        std::cout << "║              COMPRESSÃO DE ÁUDIO                   ║\n";
        std::cout << "╚════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "Arquivo entrada:  " << input_file << "\n";
        std::cout << "Arquivo saída:    " << output_file << "\n\n";
        
        std::cout << "Configurações:\n";
        std::cout << "  Golomb m:       " << (adaptive ? "adaptativo " : std::to_string(m)) << "\n";
        
        std::string pred_name;
        switch (pred) {
            case AudioCodec::PredictorType::NONE:    pred_name = "nenhum"; break;
            case AudioCodec::PredictorType::ORDER_1: pred_name = "ordem 1"; break;
            case AudioCodec::PredictorType::ORDER_2: pred_name = "ordem 2"; break;
            case AudioCodec::PredictorType::ORDER_3: pred_name = "ordem 3"; break;
        }
        std::cout << "  Preditor:       " << pred_name << "\n";
        std::cout << "  Modo sinal:     " << (sign_mode == SignMode::INTERLEAVING ? 
                                          "interleaving " : "sign-magnitude") << "\n";
        std::cout << "  Mid-Side:       " << (mid_side ? "sim " : "não") << "\n";
        std::cout << "\nProcessando...\n";
        
        if (!codec.compress(input_file, output_file, &stats)) {
            std::cerr << "\n✗ Erro na compressão!\n";
            return 1;
        }
        
        std::cout << "\n╔════════════════════════════════════════════════════╗\n";
        std::cout << "║           COMPRESSÃO CONCLUÍDA COM SUCESSO         ║\n";
        std::cout << "╚════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "Estatísticas:\n";
        std::cout << "  Tamanho original:      " << std::setw(10) << stats.original_size 
                  << " bytes\n";
        std::cout << "  Tamanho comprimido:    " << std::setw(10) << stats.compressed_size 
                  << " bytes\n";
        
        double reduction = 100.0 * (1.0 - static_cast<double>(stats.compressed_size) / 
                                           stats.original_size);
        std::cout << "  Redução:               " << std::fixed << std::setprecision(1)
                  << std::setw(10) << reduction << " %\n";
        
        std::cout << "  Taxa de compressão:    " << std::fixed << std::setprecision(2) 
                  << std::setw(10) << stats.compression_ratio << " : 1\n";
        std::cout << "  Bits por amostra:      " << std::fixed << std::setprecision(2) 
                  << std::setw(10) << stats.bits_per_sample << " bits\n";
        
        if (adaptive) {
            std::cout << "  m ótimo (canal 1):     " << std::setw(10) 
                      << stats.optimal_m << "\n";
        }
        
        std::cout << "  Tempo de codificação:  " << std::fixed << std::setprecision(2) 
                  << std::setw(10) << stats.encoding_time_ms << " ms\n";
        std::cout << "\n";
        
    } else if (mode == "-d") {
        // ==================== DESCOMPRESSÃO ====================
        
        AudioCodec codec;  // Configurações serão lidas do arquivo
        AudioCodec::CompressionStats stats;
        
        std::cout << "\n╔════════════════════════════════════════════════════╗\n";
        std::cout << "║             DESCOMPRESSÃO DE ÁUDIO                 ║\n";
        std::cout << "╚════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "Arquivo entrada:  " << input_file << "\n";
        std::cout << "Arquivo saída:    " << output_file << "\n\n";
        
        std::cout << "Processando...\n";
        
        if (!codec.decompress(input_file, output_file, &stats)) {
            std::cerr << "\n✗ Erro na descompressão!\n";
            return 1;
        }
        
        std::cout << "\n╔════════════════════════════════════════════════════╗\n";
        std::cout << "║          DESCOMPRESSÃO CONCLUÍDA COM SUCESSO       ║\n";
        std::cout << "╚════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "Estatísticas:\n";
        std::cout << "  Tempo de decodificação: " << std::fixed << std::setprecision(2) 
                  << std::setw(10) << stats.decoding_time_ms << " ms\n";
        std::cout << "\n";
        
    } else {
        std::cerr << "Erro: Modo inválido: " << mode << "\n";
        std::cerr << "Use -c para compressão ou -d para descompressão\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}