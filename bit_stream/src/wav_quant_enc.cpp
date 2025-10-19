#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include "bit_stream.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.bin> <quant_bits>\n";
        return 1;
    }

    const char* input_wav_file = argv[1];
    const char* output_bin_file = argv[2];
    int quant_bits = stoi(argv[3]);
    
    if (quant_bits <= 0 || quant_bits > 16) {
        cerr << "Invalid number of quantization bits (must be 1-16)\n";
        return 1;
    }

    // Abrir ficheiro WAV
    ifstream ifs(input_wav_file, ios::binary);
    if (!ifs) {
        cerr << "Error opening input file: " << input_wav_file << endl;
        return 1;
    }

    char header[44];
    ifs.read(header, 44);
    if (ifs.gcount() < 44) {
        cerr << "Invalid WAV file.\n";
        return 1;
    }

    uint32_t sample_rate = *reinterpret_cast<uint32_t*>(&header[24]);
    uint16_t num_channels = *reinterpret_cast<uint16_t*>(&header[22]);
    uint16_t bits_per_sample = *reinterpret_cast<uint16_t*>(&header[34]);
    uint32_t data_size = *reinterpret_cast<uint32_t*>(&header[40]);
    
    if (bits_per_sample != 16) {
        cerr << "Only 16-bit WAV files are supported.\n";
        return 1;
    }

    uint32_t num_samples = data_size / (sizeof(int16_t) * num_channels);

    cout << "Sample rate: " << sample_rate << " Hz" << endl;
    cout << "Channels: " << num_channels << endl;
    cout << "Total frames: " << num_samples << endl;
    cout << "Quantization bits: " << quant_bits << endl;


    vector<int16_t> samples(num_samples * num_channels);
    ifs.read(reinterpret_cast<char*>(samples.data()), data_size);
    ifs.close();

    ofstream ofs(output_bin_file, ios::binary);
    if (!ofs) {
        cerr << "Error opening output file: " << output_bin_file << endl;
        return 1;
    }


    ofs.write("WQ01", 4);                                      
    ofs.write(reinterpret_cast<const char*>(&sample_rate), 4); 
    ofs.write(reinterpret_cast<const char*>(&num_channels), 2);
    uint8_t qbits = static_cast<uint8_t>(quant_bits);
    ofs.write(reinterpret_cast<const char*>(&qbits), 1);       
    ofs.write(reinterpret_cast<const char*>(&num_samples), 4); 
    ofs.close();

    fstream fs(output_bin_file, ios::binary | ios::in | ios::out | ios::app);
    if (!fs) {
        cerr << "Error reopening output file for BitStream\n";
        return 1;
    }

    BitStream bs(fs, STREAM_WRITE);

    const int levels = 1 << quant_bits;

    // Quantizar e escrever cada amostra
    for (size_t i = 0; i < samples.size(); ++i) {
        float normalized = (samples[i] + 32768.0f) / 65536.0f;
        
        int q_index = static_cast<int>(normalized * levels);
        
        if (q_index >= levels) q_index = levels - 1;
        if (q_index < 0) q_index = 0;

        bs.write_n_bits(q_index, quant_bits);
    }

    bs.close();
    fs.close();


    ifstream size_check(output_bin_file, ios::binary | ios::ate);
    size_t file_size = size_check.tellg();
    size_check.close();

    size_t expected_bits = samples.size() * quant_bits;
    size_t expected_bytes = (expected_bits + 7) / 8 + 15; // +15 para o cabeçalho

    cout << "\nEncoding complete!" << endl;
    cout << "Output file: " << output_bin_file << endl;
    cout << "File size: " << file_size << " bytes" << endl;
    cout << "Expected size: ~" << expected_bytes << " bytes" << endl;
    cout << "Compression ratio: " << (data_size * 100.0 / file_size) << "%" << endl;

    return 0;
}