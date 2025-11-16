#ifndef AUDIOCODEC_H
#define AUDIOCODEC_H

#include "golomb.h"  
#include "bitstream.h"     
#include <vector>
#include <string>
#include <fstream>
#include <sndfile.h>

using SignMode = GolombCoder::SignMode;

class AudioCodec {
public:
    enum class PredictorType {
        NONE,           // Sem predição (codifica valores diretos)
        ORDER_1,        // x[n] - x[n-1]
        ORDER_2,        // 2*x[n-1] - x[n-2]
        ORDER_3         // 3*x[n-1] - 3*x[n-2] + x[n-3]
    };

    struct CompressionStats {
        size_t original_size;
        size_t compressed_size;
        double compression_ratio;
        double bits_per_sample;
        int optimal_m;
        double encoding_time_ms;
        double decoding_time_ms;
    };

private:
    int golomb_m;
    SignMode sign_mode; 
    PredictorType predictor_type;
    bool adaptive_m;  // Se true, calcula m ótimo automaticamente
    
    // Para áudio estéreo - predição inter-canal
    bool use_mid_side;  // Codificação Mid-Side


    std::vector<int> calculateResiduals(const std::vector<int>& samples, 
                                       PredictorType pred_type);
    

    std::vector<int> reconstructFromResiduals(const std::vector<int>& residuals,
                                                PredictorType pred_type);
    
    // Estima m ótimo baseado nos resíduos
    int estimateOptimalM(const std::vector<int>& residuals);
    

    void toMidSide(const std::vector<short>& left, 
                   const std::vector<short>& right,
                   std::vector<int>& mid, 
                   std::vector<int>& side);
    
    void fromMidSide(const std::vector<int>& mid,
                     const std::vector<int>& side,
                     std::vector<short>& left,
                     std::vector<short>& right);

public:
    AudioCodec(int m = 0, 
               SignMode mode = SignMode::INTERLEAVING,
               PredictorType pred = PredictorType::ORDER_1,
               bool adaptive = true,
               bool mid_side = true);
    
    // Comprime arquivo de áudio
    bool compress(const std::string& input_wav, 
                  const std::string& output_file,
                  CompressionStats* stats = nullptr);
    
    // Descomprime arquivo
    bool decompress(const std::string& input_file,
                    const std::string& output_wav,
                    CompressionStats* stats = nullptr);
    
    // Setters
    void setGolombM(int m) { golomb_m = m; }
    void setSignMode(SignMode mode) { sign_mode = mode; }
    void setPredictorType(PredictorType pred) { predictor_type = pred; }
    void setAdaptiveM(bool adaptive) { adaptive_m = adaptive; }
    void setMidSide(bool ms) { use_mid_side = ms; }
    
    // Getters
    int getGolombM() const { return golomb_m; }
    PredictorType getPredictorType() const { return predictor_type; }
};

#endif // AUDIOCODEC_H