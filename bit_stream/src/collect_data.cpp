#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>

using namespace std;
using namespace chrono;

// Estrutura para armazenar resultados de um teste
struct TestResult {
    int quant_bits;
    size_t original_size;
    size_t encoded_size;
    double compression_ratio;
    double space_savings;
    double encoding_time;
    double decoding_time;
    double total_time;
    double snr;
    double mse;
    double max_error;
};

// Função para obter tamanho de ficheiro
size_t get_file_size(const string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

// Função para fazer escape de nomes de ficheiros para shell
string escape_shell(const string& str) {
    string escaped = "'";
    for (char c : str) {
        if (c == '\'') {
            escaped += "'\\''";  // Escape single quotes
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

// Função para executar comando e capturar output
string exec_command(const string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// Extrair valor numérico de uma string (procura primeiro número)
double extract_number(const string& str) {
    stringstream ss;
    for (char c : str) {
        if (isdigit(c) || c == '.' || c == '-') {
            ss << c;
        } else if (ss.str().length() > 0) {
            break;
        }
    }
    double value;
    ss >> value;
    return ss.fail() ? -1.0 : value;
}

// Função para medir qualidade usando wav_cmp
void measure_quality(const string& original, const string& decoded, TestResult& result) {
    string cmd = "./wav_cmp " + escape_shell(original) + " " + escape_shell(decoded) + " 2>&1";
    string output = exec_command(cmd);
    
    // Procurar por SNR, MSE e Max Error no output
    istringstream iss(output);
    string line;
    
    result.snr = -1.0;
    result.mse = -1.0;
    result.max_error = -1.0;
    
    while (getline(iss, line)) {
        // Converter para lowercase para procura case-insensitive
        string line_lower = line;
        transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);
        
        if (line_lower.find("snr") != string::npos) {
            result.snr = extract_number(line);
        }
        if (line_lower.find("mse") != string::npos || 
            line_lower.find("mean") != string::npos) {
            result.mse = extract_number(line);
        }
        if (line_lower.find("max") != string::npos && 
            line_lower.find("error") != string::npos) {
            result.max_error = extract_number(line);
        }
    }
}

// Testar um nível de quantização
bool test_quantization(const string& audio_file, int quant_bits, TestResult& result) {
    result.quant_bits = quant_bits;
    
    // Nomes dos ficheiros temporários
    string encoded_file = "test_" + to_string(quant_bits) + "bit.bin";
    string decoded_file = "decoded_" + to_string(quant_bits) + "bit.wav";
    
    // Obter tamanho original
    result.original_size = get_file_size(audio_file);
    if (result.original_size == 0) {
        cerr << "Error: Could not get size of " << audio_file << endl;
        return false;
    }
    
    // ENCODING
    cout << "  Encoding with " << quant_bits << " bits... " << flush;
    
    string enc_cmd = "./wav_quant_enc " + escape_shell(audio_file) + " " + escape_shell(encoded_file) + " " + to_string(quant_bits) + " > /dev/null 2>&1";
    
    auto enc_start = high_resolution_clock::now();
    int enc_result = system(enc_cmd.c_str());
    auto enc_end = high_resolution_clock::now();
    
    if (enc_result != 0) {
        cout << "FAILED" << endl;
        return false;
    }
    
    result.encoding_time = duration<double>(enc_end - enc_start).count();
    result.encoded_size = get_file_size(encoded_file);
    
    if (result.encoded_size == 0) {
        cout << "FAILED (no output)" << endl;
        return false;
    }
    
    cout << "OK (" << fixed << setprecision(3) << result.encoding_time << "s)" << endl;
    
    // DECODING
    cout << "  Decoding... " << flush;
    
    string dec_cmd = "./wav_quant_dec " + escape_shell(encoded_file) + " " + escape_shell(decoded_file) + " > /dev/null 2>&1";
    
    auto dec_start = high_resolution_clock::now();
    int dec_result = system(dec_cmd.c_str());
    auto dec_end = high_resolution_clock::now();
    
    if (dec_result != 0) {
        cout << "FAILED" << endl;
        return false;
    }
    
    result.decoding_time = duration<double>(dec_end - dec_start).count();
    
    if (get_file_size(decoded_file) == 0) {
        cout << "FAILED (no output)" << endl;
        return false;
    }
    
    cout << "OK (" << fixed << setprecision(3) << result.decoding_time << "s)" << endl;
    
    // Calcular métricas
    result.total_time = result.encoding_time + result.decoding_time;
    result.compression_ratio = (double)result.original_size / result.encoded_size;
    result.space_savings = (1.0 - (double)result.encoded_size / result.original_size) * 100.0;
    
    cout << "  Encoded size: " << (result.encoded_size / 1024.0) << " KB" << endl;
    cout << "  Compression ratio: " << fixed << setprecision(2) << result.compression_ratio << "x" << endl;
    cout << "  Space savings: " << fixed << setprecision(1) << result.space_savings << "%" << endl;
    
    // QUALIDADE
    cout << "  Measuring quality... " << flush;
    measure_quality(audio_file, decoded_file, result);
    cout << "OK" << endl;
    
    if (result.snr > 0) {
        cout << "  SNR: " << fixed << setprecision(2) << result.snr << " dB" << endl;
    }
    if (result.mse >= 0) {
        cout << "  MSE: " << scientific << setprecision(2) << result.mse << endl;
    }
    if (result.max_error >= 0) {
        cout << "  Max Error: " << fixed << setprecision(0) << result.max_error << endl;
    }
    
    return true;
}

// Salvar resultados em CSV
void save_csv(const string& filename, const vector<TestResult>& results) {
    ofstream ofs(filename);
    if (!ofs) {
        cerr << "Error: Could not create " << filename << endl;
        return;
    }
    
    // Cabeçalho
    ofs << "Quant_Bits,Original_KB,Encoded_KB,Encoded_Bytes,Ratio,Space_Savings_%,"
        << "Enc_Time_s,Dec_Time_s,Total_Time_s,SNR_dB,MSE,Max_Error\n";
    
    // Dados
    for (const auto& r : results) {
        ofs << r.quant_bits << ","
            << fixed << setprecision(2) << (r.original_size / 1024.0) << ","
            << fixed << setprecision(2) << (r.encoded_size / 1024.0) << ","
            << r.encoded_size << ","
            << fixed << setprecision(4) << r.compression_ratio << ","
            << fixed << setprecision(2) << r.space_savings << ","
            << fixed << setprecision(4) << r.encoding_time << ","
            << fixed << setprecision(4) << r.decoding_time << ","
            << fixed << setprecision(4) << r.total_time << ",";
        
        if (r.snr > 0) {
            ofs << fixed << setprecision(2) << r.snr;
        } else {
            ofs << "N/A";
        }
        ofs << ",";
        
        if (r.mse >= 0) {
            ofs << scientific << setprecision(4) << r.mse;
        } else {
            ofs << "N/A";
        }
        ofs << ",";
        
        if (r.max_error >= 0) {
            ofs << fixed << setprecision(0) << r.max_error;
        } else {
            ofs << "N/A";
        }
        ofs << "\n";
    }
    
    ofs.close();
    cout << "\nCSV saved to: " << filename << endl;
}

// Salvar resumo em texto
void save_summary(const string& filename, const string& audio_file, const vector<TestResult>& results) {
    ofstream ofs(filename);
    if (!ofs) return;
    
    ofs << "========================================\n";
    ofs << "AUDIO CODEC TEST RESULTS\n";
    ofs << "========================================\n\n";
    ofs << "Test file: " << audio_file << "\n";
    
    if (!results.empty()) {
        ofs << "Original size: " << fixed << setprecision(2) 
            << (results[0].original_size / 1024.0) << " KB\n";
    }
    
    time_t now = time(0);
    ofs << "Test date: " << ctime(&now) << "\n";
    
    ofs << "----------------------------------------\n";
    ofs << "COMPRESSION RESULTS\n";
    ofs << "----------------------------------------\n\n";
    
    for (const auto& r : results) {
        ofs << "=== " << r.quant_bits << " bits ===\n";
        ofs << "Encoded size: " << fixed << setprecision(2) << (r.encoded_size / 1024.0) << " KB\n";
        ofs << "Compression: " << fixed << setprecision(2) << r.compression_ratio << "x\n";
        ofs << "Space saved: " << fixed << setprecision(1) << r.space_savings << "%\n";
        ofs << "Encoding time: " << fixed << setprecision(4) << r.encoding_time << "s\n";
        ofs << "Decoding time: " << fixed << setprecision(4) << r.decoding_time << "s\n";
        
        if (r.snr > 0) {
            ofs << "SNR: " << fixed << setprecision(2) << r.snr << " dB\n";
        }
        if (r.mse >= 0) {
            ofs << "MSE: " << scientific << setprecision(4) << r.mse << "\n";
        }
        if (r.max_error >= 0) {
            ofs << "Max Error: " << fixed << setprecision(0) << r.max_error << "\n";
        }
        ofs << "\n";
    }
    
    ofs.close();
    cout << "Summary saved to: " << filename << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <audio.wav> [quant_bits...]\n";
        cerr << "Example: " << argv[0] << " sample.wav 4 8 12 16\n";
        cerr << "         " << argv[0] << " sample.wav (tests all: 4,6,8,10,12,14,16)\n";
        return 1;
    }
    
    string audio_file = argv[1];
    
    // Verificar se o ficheiro existe
    if (get_file_size(audio_file) == 0) {
        cerr << "Error: File " << audio_file << " not found or empty!\n";
        return 1;
    }
    
    // Determinar quais bits testar
    vector<int> quant_bits_to_test;
    
    if (argc > 2) {
        // Bits especificados pelo utilizador
        for (int i = 2; i < argc; i++) {
            quant_bits_to_test.push_back(atoi(argv[i]));
        }
    } else {
        // Testar todos os níveis padrão
        quant_bits_to_test = {4, 6, 8, 10, 12, 14, 16};
    }
    
    cout << "==========================================\n";
    cout << "Data Collection for Audio Codec\n";
    cout << "==========================================\n";
    cout << "Audio file: " << audio_file << "\n";
    cout << "Original size: " << (get_file_size(audio_file) / 1024.0) << " KB\n";
    cout << "Testing quantization levels: ";
    for (size_t i = 0; i < quant_bits_to_test.size(); i++) {
        cout << quant_bits_to_test[i];
        if (i < quant_bits_to_test.size() - 1) cout << ", ";
    }
    cout << "\n\n";
    
    // Executar testes
    vector<TestResult> results;
    
    for (int bits : quant_bits_to_test) {
        cout << "----------------------------------------\n";
        cout << "Testing with " << bits << " bits\n";
        cout << "----------------------------------------\n";
        
        TestResult result;
        if (test_quantization(audio_file, bits, result)) {
            results.push_back(result);
        } else {
            cerr << "Warning: Test with " << bits << " bits failed!\n";
        }
        cout << "\n";
    }
    
    if (results.empty()) {
        cerr << "Error: No successful tests!\n";
        return 1;
    }
    
    // Salvar resultados
    cout << "==========================================\n";
    cout << "Saving Results\n";
    cout << "==========================================\n";
    
    // Extrair nome base do ficheiro
    string base_name = audio_file;
    size_t last_dot = base_name.find_last_of('.');
    if (last_dot != string::npos) {
        base_name = base_name.substr(0, last_dot);
    }
    size_t last_slash = base_name.find_last_of('/');
    if (last_slash != string::npos) {
        base_name = base_name.substr(last_slash + 1);
    }
    
    string csv_file = "results_" + base_name + ".csv";
    string summary_file = "summary_" + base_name + ".txt";
    
    save_csv(csv_file, results);
    save_summary(summary_file, audio_file, results);
    
    cout << "\n==========================================\n";
    cout << "Data Collection Complete!\n";
    cout << "==========================================\n";
    cout << "Results: " << results.size() << " successful tests\n";
    cout << "\nNext steps:\n";
    cout << "  1. Open " << csv_file << " in Excel/LibreOffice\n";
    cout << "  2. Create tables and graphs\n";
    cout << "  3. Listen to decoded_*.wav files\n";
    
    return 0;
}