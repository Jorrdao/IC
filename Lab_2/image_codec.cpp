#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <chrono>      
#include <filesystem> 
#include <iomanip>     
#include <cstdio>  
#include <opencv2/opencv.hpp>

#include "golomb.h"
#include "bitstream.h"

/**
 * @brief Encodes a grayscale image into a binary file.
 * @param inputFile Path to the input grayscale image (e.g., .png).
 * @param outputFile Path to the output binary file.
 * @param m The Golomb parameter 'm'.
 */
void encodeImage(const std::string& inputFile, const std::string& outputFile, int m) {
    // 1. Read the image as grayscale
    cv::Mat img = cv::imread(inputFile, cv::IMREAD_GRAYSCALE);
    if (img.empty()) {
        throw std::runtime_error("Error: Could not read input image: " + inputFile);
    }

    int rows = img.rows;
    int cols = img.cols;

    // 2. Initialize Golomb coder and BitStream writer
    GolombCoder g(m);
    BitStreamWriter writer;

    // 3. Write image headers (dimensions) to the bitstream
    // We'll use 16 bits for each dimension, allowing for images up to 65535x65535
    writer.writeBits(rows, 16);
    writer.writeBits(cols, 16);

    std::cout << "Encoding " << rows << "x" << cols << " image with m=" << m << "..." << std::endl;

    // 4. Iterate over each pixel, predict, calculate residual, and encode
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            // Get the actual pixel value
            int actual = (int)img.at<uint8_t>(y, x);
            
            // Predict the pixel value
            int predicted;
            if (y == 0 && x == 0) {
                // First pixel, no predictor
                predicted = 0;
            } else if (y == 0) {
                // First row, predict from the left
                predicted = (int)img.at<uint8_t>(y, x - 1);
            } else if (x == 0) {
                // First column, predict from above
                predicted = (int)img.at<uint8_t>(y - 1, x);
            } else {
                // Use the average of left and top pixels as predictor
                predicted = ((int)img.at<uint8_t>(y, x - 1) + (int)img.at<uint8_t>(y - 1, x)) / 2;
            }

            // Calculate prediction residual
            int residual = actual - predicted;

            // 5. Encode the residual
            // Interleaving is good for values centered around 0
            g.encode(writer, residual, GolombCoder::SignMode::INTERLEAVING);
        }
    }

    // 6. Flush the writer and write the buffer to a file
    writer.flush();
    const auto& buffer = writer.getBuffer();
    
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Error: Could not open output file: " + outputFile);
    }
    outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    
    std::cout << "Encoding complete. Output size: " << buffer.size() << " bytes." << std::endl;
}

/**
 * @brief Decodes a binary file back into a grayscale image.
 * @param inputFile Path to the input binary file.
 * @param outputFile Path to the output grayscale image (e.g., .png).
 * @param m The Golomb parameter 'm' used during encoding.
 */
void decodeImage(const std::string& inputFile, const std::string& outputFile, int m) {
    // 1. Read the entire binary file into a buffer
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile) {
        throw std::runtime_error("Error: Could not open input file: " + inputFile);
    }
    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(inFile)),
        std::istreambuf_iterator<char>()
    );

    // 2. Initialize BitStream reader and Golomb coder
    BitStreamReader reader(buffer);
    GolombCoder g(m);

    // 3. Read image headers
    int rows = (int)reader.readBits(16);
    int cols = (int)reader.readBits(16);

    if (rows == 0 || cols == 0) {
         throw std::runtime_error("Error: Invalid image dimensions read from file.");
    }

    std::cout << "Decoding " << rows << "x" << cols << " image with m=" << m << "..." << std::endl;

    // 4. Create the output image
    cv::Mat img(rows, cols, CV_8UC1);

    // 5. Iterate, predict, decode residual, and reconstruct image
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            
            // Predict the pixel value based on *already reconstructed* pixels
            int predicted;
            if (y == 0 && x == 0) {
                predicted = 0;
            } else if (y == 0) {
                predicted = (int)img.at<uint8_t>(y, x - 1);
            } else if (x == 0) {
                predicted = (int)img.at<uint8_t>(y - 1, x);
            } else {
                predicted = ((int)img.at<uint8_t>(y, x - 1) + (int)img.at<uint8_t>(y - 1, x)) / 2;
            }

            // 6. Decode the residual
            int residual = g.decode(reader, GolombCoder::SignMode::INTERLEAVING);

            // 7. Reconstruct the pixel value
            int actual = predicted + residual;

            // Clamp the value to the valid 8-bit range [0, 255]
            if (actual < 0) actual = 0;
            if (actual > 255) actual = 255;

            // Store the reconstructed pixel
            img.at<uint8_t>(y, x) = (uint8_t)actual;
        }
    }

    // 8. Save the reconstructed image
    if (!cv::imwrite(outputFile, img)) {
        throw std::runtime_error("Error: Could not write output image: " + outputFile);
    }

    std::cout << "Decoding complete. Image saved to " << outputFile << std::endl;
}

/**
 * @brief Prints usage instructions.
 */
void printUsage() {
    std::cerr << "Usage: " << std::endl;
    std::cerr << "  To encode: ./image_codec -e <input_image.ppm> <output_file.bin> <m>" << std::endl;
    std::cerr << "  To decode: ./image_codec -d <input_file.bin> <output_image.ppm> <m>" << std::endl;
    std::cerr << "  <m> is the Golomb parameter (e.g., 32, 64, 128)" << std::endl;
}


int main(int argc, char* argv[]) {
    if (argc != 5) {
        printUsage();
        return 1;
    }

    std::string mode = argv[1];
    std::string inputFile = argv[2];
    std::string outputFile = argv[3];
    int m;

    try {
        m = std::stoi(argv[4]);
        if (m <= 0) {
             throw std::invalid_argument("m must be > 0");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid m parameter '" << argv[4] << "'. " << e.what() << std::endl;
        printUsage();
        return 1;
    }

    try {
        if (mode == "-e") {
            encodeImage(inputFile, outputFile, m);
        } else if (mode == "-d") {
            decodeImage(inputFile, outputFile, m);
        } else {
            std::cerr << "Error: Unknown mode '" << mode << "'" << std::endl;
            printUsage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

#if 0
long long getFileSize(const std::string& filepath) {
    try {
        // std::filesystem::file_size returns uintmax_t, cast to long long
        return (long long)std::filesystem::file_size(filepath);
    } catch (std::filesystem::filesystem_error& e) {
        std::cerr << "  Error getting file size for " << filepath << ": " << e.what() << std::endl;
        return -1;
    }
}

/**
 * @brief Main function for batch testing and report generation.
 */
int main() {
    // --- 1. CONFIGURE YOUR TESTS HERE ---
    // Add the paths to your 5 test images
    std::vector<std::string> imageFiles = {
        "imagens_PPM/monarch.ppm",
        "imagens_PPM/airplane.ppm",
        "imagens_PPM/tulips.ppm",
        "imagens_PPM/peppers.ppm",
        "imagens_PPM/house.ppm",
        "imagens_PPM/lena.ppm",
        "imagens_PPM/girl.ppm",
        "imagens_PPM/boat.ppm",
        "imagens_PPM/baboon.ppm",
        "imagens_PPM/arial.ppm"
    };

    // Add the 5 m-values you want to test
    std::vector<int> mValues = { 16, 32, 64, 128, 256 };

    std::string csvReportFile = "compression_report.csv";
    // --- End Configuration ---


    std::ofstream csvFile(csvReportFile);
    if (!csvFile.is_open()) {
        std::cerr << "Error: Could not open CSV file for writing: " << csvReportFile << std::endl;
        return 1;
    }

    // Write CSV Header
    csvFile << "ImageName,M_Value,OriginalSize_Bytes,CompressedSize_Bytes,CompressionRatio,EncodeTime_ms,DecodeTime_ms,TotalTime_ms\n";

    std::cout << "Starting batch compression test..." << std::endl;

    for (const auto& imagePath : imageFiles) {
        for (int m : mValues) {
            std::cout << "Testing '" << imagePath << "' with m=" << m << "..." << std::endl;

            std::string compressedFile = "temp_test.bin";
            std::string decodedFile = "temp_test_dec.png"; // Use .png for safe decoding

            try {
                // --- Get Original Size ---
                // We read the image to find its uncompressed size in memory
                // (rows * cols * 1 byte), as this is the true baseline,
                // not the size of the .png or .ppm (which might be compressed).
                cv::Mat img = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
                if (img.empty()) {
                    std::cerr << "  Skipping: Could not read image " << imagePath << std::endl;
                    continue;
                }
                long long originalSize = (long long)img.total() * img.elemSize();
                if (originalSize <= 0) {
                    std::cerr << "  Skipping: Invalid original image size for " << imagePath << std::endl;
                    continue;
                }

                // --- Encode ---
                auto startEncode = std::chrono::high_resolution_clock::now();
                encodeImage(imagePath, compressedFile, m);
                auto endEncode = std::chrono::high_resolution_clock::now();
                long long encodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(endEncode - startEncode).count();

                long long compressedSize = getFileSize(compressedFile);
                if (compressedSize <= 0) {
                     std::cerr << "  Skipping: Invalid compressed file size." << std::endl;
                     std::remove(compressedFile.c_str()); // Clean up
                     continue;
                }

                // --- Decode ---
                auto startDecode = std::chrono::high_resolution_clock::now();
                decodeImage(compressedFile, decodedFile, m);
                auto endDecode = std::chrono::high_resolution_clock::now();
                long long decodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(endDecode - startDecode).count();

                // --- Calculate Stats ---
                double compressionRatio = (double)originalSize / compressedSize;
                long long totalTime = encodeTime + decodeTime;

                // --- Write to CSV ---
                csvFile << imagePath << ","
                        << m << ","
                        << originalSize << ","
                        << compressedSize << ","
                        << std::fixed << std::setprecision(4) << compressionRatio << ","
                        << encodeTime << ","
                        << decodeTime << ","
                        << totalTime << "\n";
                
                // --- Cleanup Temp Files ---
                std::remove(compressedFile.c_str());
                std::remove(decodedFile.c_str());

            } catch (const std::exception& e) {
                std::cerr << "  FAILED test for '" << imagePath << "' with m=" << m << ". Error: " << e.what() << std::endl;
                // Clean up failed files if they exist
                std::remove(compressedFile.c_str());
                std::remove(decodedFile.c_str());
            }
        }
    }

    csvFile.close();
    std::cout << "\nBatch test complete. Report saved to '" << csvReportFile << "'." << std::endl;
    return 0;
}
#endif