#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include "bit_stream.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.bin> <output.wav>\n";
        return 1;
    }

    const char* input_bin_file = argv[1];
    const char* output_wav_file = argv[2];

    cout << "Opening input file: " << input_bin_file << endl;

    // Abrir ficheiro binário
    fstream ifs(input_bin_file, ios::binary | ios::in);
    if (!ifs) {
        cerr << "Error opening input file\n";
        return 1;
    }

    // Verificar tamanho do ficheiro
    ifs.seekg(0, ios::end);
    size_t file_size = ifs.tellg();
    ifs.seekg(0, ios::beg);
    
    cout << "File size: " << file_size << " bytes" << endl;
    
    if (file_size < 15) {
        cerr << "File too small to be valid (needs at least 15 bytes for header)\n";
        return 1;
    }

    char magic[5] = {0};
    ifs.read(magic, 4);
    
    cout << "Magic: " << magic << endl;
    
    if (string(magic, 4) != "WQ01") {
        cerr << "Invalid file format (expected WQ01, got " << magic << ")\n";
        return 1;
    }

    uint32_t sample_rate, num_samples;
    uint16_t num_channels;
    uint8_t quant_bits;
    
    ifs.read(reinterpret_cast<char*>(&sample_rate), 4);
    ifs.read(reinterpret_cast<char*>(&num_channels), 2);
    ifs.read(reinterpret_cast<char*>(&quant_bits), 1);
    ifs.read(reinterpret_cast<char*>(&num_samples), 4);

    cout << "Sample rate: " << sample_rate << " Hz" << endl;
    cout << "Channels: " << num_channels << endl;
    cout << "Quantization bits: " << (int)quant_bits << endl;
    cout << "Number of frames: " << num_samples << endl;

    if (quant_bits == 0 || quant_bits > 16) {
        cerr << "Invalid quantization bits: " << (int)quant_bits << endl;
        return 1;
    }
    
    if (num_channels == 0 || num_channels > 8) {
        cerr << "Invalid number of channels: " << num_channels << endl;
        return 1;
    }
    
    if (num_samples == 0 || num_samples > 1000000000) {
        cerr << "Invalid number of samples: " << num_samples << endl;
        return 1;
    }

    // Calcular quantos samples totais esperamos
    size_t total_samples = (size_t)num_samples * (size_t)num_channels;
    size_t expected_bits = total_samples * quant_bits;
    size_t expected_data_bytes = (expected_bits + 7) / 8;
    size_t expected_total_size = 15 + expected_data_bytes;
    
    cout << "Expected file size: ~" << expected_total_size << " bytes" << endl;
    cout << "Actual file size: " << file_size << " bytes" << endl;
    
    if (file_size < expected_total_size - 10) {
        cerr << "Warning: File seems truncated!" << endl;
    }

    // Inicializar BitStream para leitura
    cout << "Initializing BitStream..." << endl;
    BitStream bs(ifs, STREAM_READ);

    const int levels = 1 << quant_bits;
    vector<int16_t> samples;
    samples.reserve(total_samples);

    cout << "Decoding " << total_samples << " samples..." << endl;

    size_t samples_read = 0;
    size_t progress_step = total_samples / 10;
    if (progress_step == 0) progress_step = 1;

    for (size_t i = 0; i < total_samples; ++i) {
        if (ifs.eof() || !ifs.good()) {
            cerr << "\nUnexpected end of file at sample " << i << "/" << total_samples << endl;
            break;
        }

        int q_index = bs.read_n_bits(quant_bits);
        
        if (q_index < 0 || q_index >= levels) {
            cerr << "\nInvalid quantization index at sample " << i << ": " << q_index << endl;
            q_index = (q_index < 0) ? 0 : (levels - 1);
        }
        
        float normalized = (q_index + 0.5f) / levels;
        int16_t sample = static_cast<int16_t>(normalized * 65536.0f - 32768.0f);
        samples.push_back(sample);
        
        samples_read++;
        
    }

    cout << "Decoded " << samples_read << " samples" << endl;

    bs.close();
    ifs.close();

    if (samples_read != total_samples) {
        cerr << "Warning: Expected " << total_samples << " samples, but got " << samples_read << endl;
    }

    cout << "Writing WAV file: " << output_wav_file << endl;
    
    ofstream ofs(output_wav_file, ios::binary);
    if (!ofs) {
        cerr << "Error opening output file\n";
        return 1;
    }
    
    // Construir cabeçalho WAV (44 bytes)
    uint32_t data_size = samples.size() * 2;
    uint32_t file_size_wav = 36 + data_size;
    
    ofs.write("RIFF", 4);
    ofs.write(reinterpret_cast<char*>(&file_size_wav), 4);
    ofs.write("WAVE", 4);
    ofs.write("fmt ", 4);
    uint32_t fmt_size = 16;
    ofs.write(reinterpret_cast<char*>(&fmt_size), 4);
    uint16_t audio_format = 1; // PCM
    ofs.write(reinterpret_cast<char*>(&audio_format), 2);
    ofs.write(reinterpret_cast<char*>(&num_channels), 2);
    ofs.write(reinterpret_cast<char*>(&sample_rate), 4);
    uint32_t byte_rate = sample_rate * num_channels * 2;
    ofs.write(reinterpret_cast<char*>(&byte_rate), 4);
    uint16_t block_align = num_channels * 2;
    ofs.write(reinterpret_cast<char*>(&block_align), 2);
    uint16_t bits_per_sample = 16;
    ofs.write(reinterpret_cast<char*>(&bits_per_sample), 2);
    ofs.write("data", 4);
    ofs.write(reinterpret_cast<char*>(&data_size), 4);
    
    // Escrever dados
    ofs.write(reinterpret_cast<char*>(samples.data()), data_size);
    
    ofs.close();
    
    cout << "\n✓ Decoding complete!" << endl;
    cout << "Output: " << output_wav_file << endl;
    cout << "Samples written: " << samples.size() << endl;
    
    return 0;
}