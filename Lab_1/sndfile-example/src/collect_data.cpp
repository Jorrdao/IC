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
#include <algorithm>
#include <regex> // For robust output parsing

using namespace std;
using namespace chrono;

// Define executable paths
// NOTE: Assuming all executables are in the same relative directory as the script is run from,
// or that the 'bin' directory is in the system PATH.
const string BIN_DIR = "./bin";
const string ENCODER_EXEC = BIN_DIR + "/wav_dct_enc";
const string DECODER_EXEC = BIN_DIR + "/wav_dct_dec";
const string CMP_EXEC = BIN_DIR + "/wav_cmp";

// Default parameters for the DCT codec analysis (matching analyze_codec.sh)
const size_t DEFAULT_BS = 1024;
const double DEFAULT_FRAC = 0.2;

// Structure to store results of a test
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

// --- UTILITY FUNCTIONS (Kept from original) ---

// Function to get file size
size_t get_file_size(const string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

// Function to escape filenames for shell (important for files with spaces)
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

// Function to execute command and capture output
string exec_command(const string& cmd) {
    // Note: Using popen is generally fine for tool execution in a controlled environment
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buffer[128];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// --- CORE TEST FUNCTION ---

/**
 * @brief Runs a single encode -> decode -> compare test case.
 * * @param audio_file Input WAV file path.
 * @param temp_enc_file Temporary encoded file path.
 * @param temp_dec_file Temporary decoded WAV file path.
 * @param quant_bits The number of bits for quantization (-qbits).
 * @param result Reference to store the TestResult struct.
 * @return true if the test was successful and metrics were extracted, false otherwise.
 */
bool run_test_case(const string& audio_file, const string& temp_enc_file, const string& temp_dec_file, int quant_bits, TestResult& result) {
    cout << "--- Running Test: QBits = " << quant_bits << " ---" << endl;
    
    // Set fixed parameters for the test
    int bs = DEFAULT_BS;
    double frac = DEFAULT_FRAC;

    // --- 1. Encoding (Capture Time and Execute) ---
    string enc_cmd = ENCODER_EXEC + " -bs " + to_string(bs) + 
                     " -frac " + to_string(frac) + 
                     " -qbits " + to_string(quant_bits) + 
                     " " + escape_shell(audio_file) + " " + escape_shell(temp_enc_file);
    
    auto start_enc = high_resolution_clock::now();
    string enc_output = exec_command(enc_cmd);
    auto end_enc = high_resolution_clock::now();
    result.encoding_time = duration_cast<milliseconds>(end_enc - start_enc).count() / 1000.0;
    
    // Check if encoder ran successfully (a simple file check)
    result.encoded_size = get_file_size(temp_enc_file);
    if (result.encoded_size == 0) {
        cerr << "Error: Encoder failed or produced an empty file." << endl;
        return false;
    }

    // --- 2. Decoding (Capture Time and Execute) ---
    string dec_cmd = DECODER_EXEC + " " + escape_shell(temp_enc_file) + " " + escape_shell(temp_dec_file);
    
    auto start_dec = high_resolution_clock::now();
    string dec_output = exec_command(dec_cmd);
    auto end_dec = high_resolution_clock::now();
    result.decoding_time = duration_cast<milliseconds>(end_dec - start_dec).count() / 1000.0;
    
    // Check if decoder ran successfully
    if (get_file_size(temp_dec_file) == 0) {
        cerr << "Error: Decoder failed or produced an empty file." << endl;
        return false;
    }

    result.total_time = result.encoding_time + result.decoding_time;

    // --- 3. Comparison (Get Distortion Metrics) ---
    string cmp_cmd = CMP_EXEC + " " + escape_shell(audio_file) + " " + escape_shell(temp_dec_file);
    string cmp_output = exec_command(cmp_cmd);

    // --- 4. Parse Metrics ---
    
    // Regex patterns for extraction
    // Example: SNR: 45.67 dB
    regex snr_regex("SNR:\\s+([0-9.-]+)\\s+dB");
    // Example: MSE: 0.00123
    regex mse_regex("MSE:\\s+([0-9.e-]+)");
    // Example: MAX error: 10
    regex max_err_regex("MAX error:\\s+([0-9]+)");

    smatch match;

    if (regex_search(cmp_output, match, snr_regex) && match.size() > 1) {
        result.snr = stod(match[1].str());
    } else {
        cerr << "Warning: Could not find SNR in comparator output." << endl;
    }

    if (regex_search(cmp_output, match, mse_regex) && match.size() > 1) {
        result.mse = stod(match[1].str());
    } else {
        cerr << "Warning: Could not find MSE in comparator output." << endl;
    }

    if (regex_search(cmp_output, match, max_err_regex) && match.size() > 1) {
        // Max error is an integer sample value
        result.max_error = stod(match[1].str()); 
    } else {
        cerr << "Warning: Could not find MAX error in comparator output." << endl;
    }

    // --- 5. Calculate Ratios ---
    result.original_size = get_file_size(audio_file);
    if (result.original_size > 0) {
        // Compression Ratio = Original Size / Encoded Size
        result.compression_ratio = (double)result.original_size / result.encoded_size;
        // Space Savings = 1 - (Encoded Size / Original Size)
        result.space_savings = 1.0 - ((double)result.encoded_size / result.original_size);
    } else {
        cerr << "Error: Original file size is zero. Cannot calculate ratios." << endl;
        return false;
    }

    cout << "  -> Success. SNR: " << fixed << setprecision(2) << result.snr << " dB | Ratio: " << result.compression_ratio << ":1" << endl;
    
    // Clean up temporary files (important)
    remove(temp_enc_file.c_str());
    remove(temp_dec_file.c_str());

    return true;
}

// --- REPORTING FUNCTIONS (Kept and minorly adjusted) ---

void save_csv(const string& filename, const vector<TestResult>& results) {
    ofstream ofs(filename);
    if (!ofs.is_open()) {
        cerr << "Error: Could not open CSV file " << filename << " for writing." << endl;
        return;
    }

    // Write header
    ofs << "QuantBits,OriginalSize,EncodedSize,CompressionRatio,SpaceSavings,EncodingTime_s,DecodingTime_s,TotalTime_s,SNR_dB,MSE,MaxError\n";

    // Write data
    ofs << fixed << setprecision(6);
    for (const auto& r : results) {
        ofs << r.quant_bits << ","
            << r.original_size << ","
            << r.encoded_size << ","
            << r.compression_ratio << ","
            << r.space_savings << ","
            << r.encoding_time << ","
            << r.decoding_time << ","
            << r.total_time << ","
            << r.snr << ","
            << r.mse << ","
            << r.max_error << "\n";
    }

    cout << "Results saved to " << filename << endl;
}

void save_summary(const string& filename, const string& audio_file, const vector<TestResult>& results) {
    ofstream ofs(filename);
    if (!ofs.is_open()) {
        cerr << "Error: Could not open summary file " << filename << " for writing." << endl;
        return;
    }

    ofs << "--- Compression Analysis Summary ---\n";
    ofs << "Input File: " << audio_file << "\n";
    ofs << "Original Size: " << results[0].original_size << " bytes\n";
    ofs << "DCT Block Size: " << DEFAULT_BS << "\n";
    ofs << "DCT Kept Fraction: " << DEFAULT_FRAC << "\n\n";

    ofs << "-----------------------------------------------------------------------------------------\n";
    ofs << " QBits | Ratio:1 | Space Saved | SNR (dB) | MSE (x10^-6) | Max Err | Enc Time (s) | Dec Time (s) \n";
    ofs << "-----------------------------------------------------------------------------------------\n";
    
    ofs << fixed << setprecision(2);
    for (const auto& r : results) {
        ofs << setw(5) << r.quant_bits << " |"
            << setw(7) << r.compression_ratio << " |"
            << setw(11) << (r.space_savings * 100.0) << "% |"
            << setw(8) << r.snr << " |"
            << setw(12) << (r.mse * 1000000.0) << " |"
            << setw(7) << r.max_error << " |"
            << setw(12) << r.encoding_time << " |"
            << setw(12) << r.decoding_time << " \n";
    }
    ofs << "-----------------------------------------------------------------------------------------\n";
    
    cout << "Summary saved to " << filename << endl;
}

// --- MAIN EXECUTION ---

int main(int argc, char *argv[]) {
    // We are testing the effect of varying the quantization bits
    // This array matches the QBITS_TESTS from your original shell script.
    const vector<int> QBITS_TO_TEST = { 32, 16, 8, 4 };
    
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <wavFileIn>" << endl;
        cerr << "Note: This script assumes 'wav_dct_enc', 'wav_dct_dec', and 'wav_cmp' are in ./bin/." << endl;
        return 1;
    }

    string audio_file = argv[1];
    if (get_file_size(audio_file) == 0) {
        cerr << "Error: Input file " << audio_file << " not found or is empty!" << endl;
        return 1;
    }
    
    // Define temporary file names
    const string temp_enc_file = audio_file + ".temp.enc";
    const string temp_dec_file = audio_file + ".temp.out.wav";

    // Clean up any stale temporary files from previous runs
    remove(temp_enc_file.c_str());
    remove(temp_dec_file.c_str());
    
    vector<TestResult> results;
    
    cout << "==========================================\n";
    cout << "Starting Data Collection for: " << audio_file << endl;
    cout << "Tests: " << QBITS_TO_TEST.size() << " different quantization levels." << endl;
    cout << "==========================================\n";

    // Loop through all quantization bit levels
    for (int bits : QBITS_TO_TEST) {
        TestResult result;
        result.quant_bits = bits;
        
        if (run_test_case(audio_file, temp_enc_file, temp_dec_file, bits, result)) {
            results.push_back(result);
        } else {
            cerr << "Warning: Test with " << bits << " bits failed!" << endl;
        }
        cout << "\n";
    }
    
    if (results.empty()) {
        cerr << "Error: No successful tests! Cannot generate report." << endl;
        return 1;
    }
    
    // --- Post-processing and Saving Results ---
    
    // Extract base name of the file for output
    string base_name = audio_file;
    size_t last_slash = base_name.find_last_of('/');
    if (last_slash != string::npos) {
        base_name = base_name.substr(last_slash + 1);
    }
    size_t last_dot = base_name.find_last_of('.');
    if (last_dot != string::npos) {
        base_name = base_name.substr(0, last_dot);
    }
    
    string csv_file = "results_" + base_name + ".csv";
    string summary_file = "summary_" + base_name + ".txt";
    
    cout << "==========================================\n";
    cout << "Saving Results\n";
    cout << "==========================================\n";
    
    save_csv(csv_file, results);
    save_summary(summary_file, audio_file, results);
    
    cout << "\n==========================================\n";
    cout << "Data Collection Complete!\n";
    cout << "==========================================\n";
    cout << "Results: " << results.size() << " successful tests\n";

    return 0;
}
