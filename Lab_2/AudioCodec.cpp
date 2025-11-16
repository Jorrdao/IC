#include "AudioCodec.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstring>

AudioCodec::AudioCodec(int m, GolombCoder::SignMode mode, PredictorType pred, 
                       bool adaptive, bool mid_side)
    : golomb_m(m), sign_mode(mode), predictor_type(pred), 
      adaptive_m(adaptive), use_mid_side(mid_side) {}

std::vector<int> AudioCodec::calculateResiduals(const std::vector<int>& samples,
                                                PredictorType pred_type) {
    std::vector<int> residuals;
    residuals.reserve(samples.size());
    
    switch (pred_type) {
        case PredictorType::NONE:
            residuals = samples;
            break;
            
        case PredictorType::ORDER_1:

            if (!samples.empty()) {
                residuals.push_back(samples[0]);
            }
            // Preditor: x[n] = x[n-1]
            for (size_t i = 1; i < samples.size(); i++) {
                int predicted = samples[i-1];
                int residual = samples[i] - predicted;
                residuals.push_back(residual);
            }
            break;
            
        case PredictorType::ORDER_2:
            // Primeiros dois samples sem predição
            if (samples.size() >= 1) {
                residuals.push_back(samples[0]);
            }
            if (samples.size() >= 2) {
                residuals.push_back(samples[1]);
            }
            // Preditor: x[n] = 2*x[n-1] - x[n-2]
            for (size_t i = 2; i < samples.size(); i++) {
                int predicted = 2 * samples[i-1] - samples[i-2];
                int residual = samples[i] - predicted;
                residuals.push_back(residual);
            }
            break;
            
        case PredictorType::ORDER_3:
            // Primeiros três samples sem predição
            for (size_t i = 0; i < std::min(size_t(3), samples.size()); i++) {
                residuals.push_back(samples[i]);
            }
            // Preditor: x[n] = 3*x[n-1] - 3*x[n-2] + x[n-3]
            for (size_t i = 3; i < samples.size(); i++) {
                int predicted = 3 * samples[i-1] - 3 * samples[i-2] + samples[i-3];
                int residual = samples[i] - predicted;
                residuals.push_back(residual);
            }
            break;
    }
    
    return residuals;
}

std::vector<int> AudioCodec::reconstructFromResiduals(
    const std::vector<int>& residuals, PredictorType pred_type) {
    
    std::vector<int> samples; // Usa INT para a reconstrução
    samples.reserve(residuals.size());
    
    switch (pred_type) {
        case PredictorType::NONE:
            samples = residuals; // Copia direta de int
            break;
            
        case PredictorType::ORDER_1:
            if (!residuals.empty()) {
                samples.push_back(residuals[0]);
            }
            for (size_t i = 1; i < residuals.size(); i++) {
                int predicted = samples[i-1]; 
                int sample = predicted + residuals[i];
                samples.push_back(sample); 
            }
            break;
            
        case PredictorType::ORDER_2:
            if (residuals.size() >= 1) {
                samples.push_back(residuals[0]);
            }
            if (residuals.size() >= 2) {
                samples.push_back(residuals[1]);
            }
            for (size_t i = 2; i < residuals.size(); i++) {
                int predicted = 2 * samples[i-1] - samples[i-2];
                int sample = predicted + residuals[i];
                samples.push_back(sample);
            }
            break;
            
        case PredictorType::ORDER_3:
            for (size_t i = 0; i < std::min(size_t(3), residuals.size()); i++) {
                samples.push_back(residuals[i]);
            }
            for (size_t i = 3; i < residuals.size(); i++) {
                int predicted = 3 * samples[i-1] - 3 * samples[i-2] + samples[i-3];
                int sample = predicted + residuals[i];
                samples.push_back(sample);
            }
            break;
    }
    
    return samples; // Retorna o vector de INTs
}

// Esta função estava correta, não foi alterada
int AudioCodec::estimateOptimalM(const std::vector<int>& residuals) {
    if (residuals.empty()) return 10;
    
    // Calcula a média dos valores absolutos
    double sum = 0.0;
    for (int r : residuals) {
        sum += std::abs(r);
    }
    double mean = sum / residuals.size();
    
    // m ótimo para distribuição geométrica: m ≈ ceil(0.69 * mean)
    int m = static_cast<int>(std::ceil(0.69 * mean));
    
    // Limites razoáveis
    if (m < 1) m = 1;
    if (m > 1024) m = 1024;
    
    return m;
}

void AudioCodec::toMidSide(const std::vector<short>& left,
                          const std::vector<short>& right,
                          std::vector<int>& mid,
                          std::vector<int>& side) {
    mid.resize(left.size());
    side.resize(left.size());
    
    for (size_t i = 0; i < left.size(); i++) {
        int l = static_cast<int>(left[i]);
        int r = static_cast<int>(right[i]);
        
        side[i] = l - r;

        mid[i] = (l + r) >> 1; 
    }
}


void AudioCodec::fromMidSide(const std::vector<int>& mid,
                            const std::vector<int>& side,
                            std::vector<short>& left,
                            std::vector<short>& right) {
    left.resize(mid.size());
    right.resize(mid.size());
    
    for (size_t i = 0; i < mid.size(); i++) {
        // A lógica de reconstrução agora opera em 'int'
        int m = mid[i];
        int s = side[i];
        int r_rec = m - (s >> 1); // Usa '>> 1' (divisão floor)
        int l_rec = r_rec + s;


        left[i] = static_cast<short>(l_rec);
        right[i] = static_cast<short>(r_rec);
    }
}

bool AudioCodec::compress(const std::string& input_wav,
                         const std::string& output_file,
                         CompressionStats* stats) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Abrir arquivo WAV com libsndfile
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    
    SNDFILE* infile = sf_open(input_wav.c_str(), SFM_READ, &sfinfo);
    if (!infile) {
        std::cerr << "Erro ao abrir arquivo: " << input_wav << std::endl;
        std::cerr << "Erro: " << sf_strerror(NULL) << std::endl;
        return false;
    }
    
    // Validações
    if (sfinfo.channels > 2) {
        std::cerr << "Suporta apenas mono ou estéreo" << std::endl;
        sf_close(infile);
        return false;
    }
    
    // Ler todas as amostras
    std::vector<short> samples(sfinfo.frames * sfinfo.channels);
    sf_count_t read = sf_readf_short(infile, samples.data(), sfinfo.frames);
    sf_close(infile);
    
    if (read != sfinfo.frames) {
        std::cerr << "Erro ao ler samples" << std::endl;
        return false;
    }
    
    // Separar canais
    std::vector<short> left, right;
    if (sfinfo.channels == 1) {
        // Em mono, 'left' contém todas as amostras
        left.resize(sfinfo.frames);
        for (sf_count_t i = 0; i < sfinfo.frames; i++) {
            left[i] = samples[i];
        }
    } else {
        left.reserve(sfinfo.frames);
        right.reserve(sfinfo.frames);
        for (sf_count_t i = 0; i < sfinfo.frames; i++) {
            left.push_back(samples[i * 2]);
            right.push_back(samples[i * 2 + 1]);
        }
    }
    
    std::vector<int> ch1, ch2;
    if (sfinfo.channels == 2 && use_mid_side) {
        toMidSide(left, right, ch1, ch2); // (short, short) -> (int, int)
    } else {
        // Copia de short para int
        ch1.assign(left.begin(), left.end());
        if (sfinfo.channels == 2) {
            ch2.assign(right.begin(), right.end());
        }
    }
    
    // Calcular resíduos
    // [CORRIGIDO] Chama a versão (int) -> (int)
    auto residuals_ch1 = calculateResiduals(ch1, predictor_type);
    std::vector<int> residuals_ch2;
    if (sfinfo.channels == 2) {
        residuals_ch2 = calculateResiduals(ch2, predictor_type);
    }
    
    // Determinar m
    int m_ch1 = adaptive_m ? estimateOptimalM(residuals_ch1) : (golomb_m > 0 ? golomb_m : 1);
    int m_ch2 = m_ch1;
    if (sfinfo.channels == 2 && adaptive_m) {
        m_ch2 = estimateOptimalM(residuals_ch2);
    }
    
    // Escrever arquivo comprimido
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Erro ao criar arquivo de saída" << std::endl;
        return false;
    }
    
    // Header (não alterado)
    out.write("GLMB", 4);  // Magic number
    uint8_t version = 1;
    out.write(reinterpret_cast<char*>(&version), 1);
    
    uint32_t samplerate = sfinfo.samplerate;
    uint16_t channels = sfinfo.channels;
    uint16_t bits = 16;
    uint64_t frames = sfinfo.frames;
    
    out.write(reinterpret_cast<char*>(&samplerate), 4);
    out.write(reinterpret_cast<char*>(&channels), 2);
    out.write(reinterpret_cast<char*>(&bits), 2);
    out.write(reinterpret_cast<char*>(&frames), 8);
    
    uint8_t pred_byte = static_cast<uint8_t>(predictor_type);
    uint8_t sign_byte = static_cast<uint8_t>(sign_mode);
    uint8_t ms_byte = (sfinfo.channels == 2 && use_mid_side) ? 1 : 0; // Só marca MS se for estéreo
    uint8_t adaptive_byte = adaptive_m ? 1 : 0;
    
    out.write(reinterpret_cast<char*>(&pred_byte), 1);
    out.write(reinterpret_cast<char*>(&sign_byte), 1);
    out.write(reinterpret_cast<char*>(&ms_byte), 1);
    out.write(reinterpret_cast<char*>(&adaptive_byte), 1);
    
    if (!adaptive_m) {
        uint32_t fixed_m = m_ch1; // m_ch1 foi definido para golomb_m
        out.write(reinterpret_cast<char*>(&fixed_m), 4);
    }
    
    // Codificar canal 1
    BitStreamWriter writer;
    GolombCoder coder1(m_ch1);
    
    if (adaptive_m) {
        uint32_t m_write = m_ch1;
        out.write(reinterpret_cast<char*>(&m_write), 4);
    }
    
    for (int res : residuals_ch1) {
        coder1.encode(writer, res, sign_mode);
    }
    
    // Codificar canal 2 (se existir)
    if (sfinfo.channels == 2) {
        GolombCoder coder2(m_ch2);
        
        if (adaptive_m) {
            uint32_t m_write = m_ch2;
            out.write(reinterpret_cast<char*>(&m_write), 4);
        }
        
        for (int res : residuals_ch2) {
            coder2.encode(writer, res, sign_mode);
        }
    }
    
    // Escrever dados codificados
    const auto& encoded = writer.getBuffer();
    uint64_t data_size = encoded.size();
    out.write(reinterpret_cast<char*>(&data_size), 8);
    out.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    
    out.close();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Estatísticas (corrigido para ser mais robusto)
    if (stats) {
        stats->original_size = sfinfo.frames * sfinfo.channels * sizeof(short);
        
        size_t header_size = 4 + 1 + 4 + 2 + 2 + 8 + 1 + 1 + 1 + 1; // 25 bytes base
        if (!adaptive_m) {
            header_size += 4; // m fixo
        } else {
            header_size += 4; // m_ch1
            if (sfinfo.channels == 2) {
                header_size += 4; // m_ch2
            }
        }
        header_size += 8; // data_size
        
        stats->compressed_size = header_size + encoded.size();
        
        if (stats->original_size > 0) {
            stats->compression_ratio = static_cast<double>(stats->original_size) / 
                                       stats->compressed_size;
        } else {
            stats->compression_ratio = 0;
        }

        if (sfinfo.frames * sfinfo.channels > 0) {
            stats->bits_per_sample = (stats->compressed_size * 8.0) / 
                                     (sfinfo.frames * sfinfo.channels);
        } else {
            stats->bits_per_sample = 0;
        }
        
        stats->optimal_m = m_ch1;
        stats->encoding_time_ms = 
            std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }
    
    return true;
}

bool AudioCodec::decompress(const std::string& input_file,
                           const std::string& output_wav,
                           CompressionStats* stats) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ifstream in(input_file, std::ios::binary);
    if (!in) {
        std::cerr << "Erro ao abrir arquivo comprimido" << std::endl;
        return false;
    }
    
    // Ler header (não alterado)
    char magic[4];
    in.read(magic, 4);
    if (std::strncmp(magic, "GLMB", 4) != 0) {
        std::cerr << "Formato de arquivo inválido" << std::endl;
        return false;
    }
    
    uint8_t version;
    in.read(reinterpret_cast<char*>(&version), 1);
    
    uint32_t samplerate;
    uint16_t channels, bits;
    uint64_t frames;
    
    in.read(reinterpret_cast<char*>(&samplerate), 4);
    in.read(reinterpret_cast<char*>(&channels), 2);
    in.read(reinterpret_cast<char*>(&bits), 2);
    in.read(reinterpret_cast<char*>(&frames), 8);
    
    uint8_t pred_byte, sign_byte, ms_byte, adaptive_byte;
    in.read(reinterpret_cast<char*>(&pred_byte), 1);
    in.read(reinterpret_cast<char*>(&sign_byte), 1);
    in.read(reinterpret_cast<char*>(&ms_byte), 1);
    in.read(reinterpret_cast<char*>(&adaptive_byte), 1);
    
    PredictorType pred = static_cast<PredictorType>(pred_byte);
    SignMode mode = static_cast<SignMode>(sign_byte);
    bool ms = (ms_byte == 1);
    bool adaptive = (adaptive_byte == 1);
    
    int m_ch1, m_ch2 = 0; // m_ch2 inicializado
    if (!adaptive) {
        uint32_t fixed_m;
        in.read(reinterpret_cast<char*>(&fixed_m), 4);
        m_ch1 = m_ch2 = fixed_m;
    } else {
        uint32_t m_read;
        in.read(reinterpret_cast<char*>(&m_read), 4);
        m_ch1 = m_read;
        if (channels == 2) {
            in.read(reinterpret_cast<char*>(&m_read), 4);
            m_ch2 = m_read;
        }
    }
    
    uint64_t data_size;
    in.read(reinterpret_cast<char*>(&data_size), 8);
    
    std::vector<uint8_t> encoded_data(data_size);
    in.read(reinterpret_cast<char*>(encoded_data.data()), data_size);
    in.close();
    
    // Decodificar (não alterado)
    BitStreamReader reader(encoded_data);
    GolombCoder coder1(m_ch1);
    
    std::vector<int> residuals_ch1, residuals_ch2;
    residuals_ch1.reserve(frames);
    
    for (uint64_t i = 0; i < frames; i++) {
        residuals_ch1.push_back(coder1.decode(reader, mode));
    }
    
    if (channels == 2) {
        GolombCoder coder2(m_ch2);
        residuals_ch2.reserve(frames);
        for (uint64_t i = 0; i < frames; i++) {
            residuals_ch2.push_back(coder2.decode(reader, mode));
        }
    }
    
    auto ch1 = reconstructFromResiduals(residuals_ch1, pred); // (int) -> (int)
    std::vector<int> ch2;
    if (channels == 2) {
        ch2 = reconstructFromResiduals(residuals_ch2, pred); // (int) -> (int)
    }
    
    std::vector<short> left, right;
    if (channels == 2 && ms) {
        fromMidSide(ch1, ch2, left, right);
    } else {
        // Converte de int para short
        left.resize(ch1.size());
        for(size_t i = 0; i < ch1.size(); ++i) {
            left[i] = static_cast<short>(ch1[i]);
        }
        if (channels == 2) {
            right.resize(ch2.size());
            for(size_t i = 0; i < ch2.size(); ++i) {
                right[i] = static_cast<short>(ch2[i]);
            }
        }
    }
    
    // Intercalar canais
    std::vector<short> output;
    if (channels == 1) {
        output = left;
    } else {
        output.reserve(frames * 2);
        for (size_t i = 0; i < frames; i++) {
            output.push_back(left[i]);
            output.push_back(right[i]);
        }
    }
    
    // Escrever WAV (não alterado)
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = samplerate;
    sfinfo.channels = channels;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
    SNDFILE* outfile = sf_open(output_wav.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        std::cerr << "Erro ao criar arquivo WAV" << std::endl;
        std::cerr << "Erro: " << sf_strerror(NULL) << std::endl;
        return false;
    }
    
    sf_writef_short(outfile, output.data(), frames);
    sf_close(outfile);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (stats) {
        stats->decoding_time_ms = 
            std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }
    
    return true;
}