#include <iostream>
#include <vector>
#include <iomanip> // For std::setw
#include "golomb.h"
#include "bitstream.h"

// Helper function to print a byte buffer as bits
void printBufferAsBits(const std::vector<uint8_t>& buffer) {
    for (uint8_t byte : buffer) {
        for (int i = 7; i >= 0; --i) {
            std::cout << ((byte >> i) & 1);
        }
        std::cout << " ";
    }
    std::cout << std::endl;
}

// Helper function to run a full encode/decode test
bool runTest(int m, const std::vector<int>& test_data, GolombCoder::SignMode mode) {
    std::string mode_str = (mode == GolombCoder::SignMode::INTERLEAVING) ? "INTERLEAVING" : "SIGN_MAGNITUDE";
    std::cout << "--- Testing m = " << m << " with " << mode_str << " ---" << std::endl;

    GolombCoder coder(m);
    BitStreamWriter writer;

    // --- ENCODE ---
    std::cout << "Encoding values: ";
    for (int val : test_data) {
        std::cout << val << " ";
        coder.encode(writer, val, mode);
    }
    std::cout << std::endl;
    writer.flush();
    const auto& buffer = writer.getBuffer();
    std::cout << "Resulting bitstream: ";
    printBufferAsBits(buffer);

    // --- DECODE ---
    BitStreamReader reader(buffer);
    std::vector<int> decoded_data;
    std::cout << "Decoding values: ";
    bool success = true;

    for (size_t i = 0; i < test_data.size(); ++i) {
        int decoded_val = coder.decode(reader, mode);
        decoded_data.push_back(decoded_val);
        std::cout << decoded_val << " ";

        if (decoded_val != test_data[i]) {
            success = false;
        }
    }
    std::cout << std::endl;

    // --- VERIFY ---
    if (success) {
        std::cout << "Result: SUCCESS!\n" << std::endl;
    } else {
        std::cout << "Result: FAILED!\n" << std::endl;
    }
    return success;
}

int main() {
    // A list of numbers to test our coder with
    std::vector<int> test_data = {0, 1, -1, 2, -2, 3, -3, 8, -8, 15, -15, 23, -42};

    bool all_passed = true;

    // Test with m=5 (not a power of 2, general case)
    all_passed &= runTest(5, test_data, GolombCoder::SignMode::INTERLEAVING);
    all_passed &= runTest(5, test_data, GolombCoder::SignMode::SIGN_MAGNITUDE);

    // Test with m=8 (a power of 2, Rice code case)
    all_passed &= runTest(8, test_data, GolombCoder::SignMode::INTERLEAVING);
    all_passed &= runTest(8, test_data, GolombCoder::SignMode::SIGN_MAGNITUDE);

    // Test with m=1 (special case, unary)
    all_passed &= runTest(1, test_data, GolombCoder::SignMode::INTERLEAVING);
    all_passed &= runTest(1, test_data, GolombCoder::SignMode::SIGN_MAGNITUDE);


    if (all_passed) {
        std::cout << "--- All tests passed! ---" << std::endl;
    } else {
        std::cout << "--- SOME TESTS FAILED! ---" << std::endl;
    }

    return 0;
}