#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

// --- DEFINE YOUR FOLDER PATHS HERE ---
// Note the trailing slash '/'
const std::string INPUT_DIR = "imagens_PPM/";
const std::string OUTPUT_DIR = "Result_Images/";

int main(int argc, char** argv) {
    // 1. Check and parse command-line arguments
    if (argc != 4) {
        // Updated usage message
        std::cerr << "Usage: " << argv[0] << " <input_filename> <output_filename> <channel_number>" << std::endl;
        std::cerr << "Example: " << argv[0] << " girl.ppm girl_blue.png 0" << std::endl;
        std::cerr << "Channel number: 0 for Blue, 1 for Green, 2 for Red" << std::endl;
        return -1;
    }

    // --- Build the full paths ---
    std::string input_filename = INPUT_DIR + argv[1];
    std::string output_filename = OUTPUT_DIR + argv[2];
    int channel_to_extract = std::stoi(argv[3]); // 0=B, 1=G, 2=R

    if (channel_to_extract < 0 || channel_to_extract > 2) {
        std::cerr << "Error: Channel number must be 0, 1, or 2." << std::endl;
        return -1;
    }

    // 2. Read the source image
    cv::Mat input_image = cv::imread(input_filename);
    if (input_image.empty()) {
        std::cerr << "Error: Could not read input image " << input_filename << std::endl;
        std::cerr << "(Check if the file exists and the path is correct)" << std::endl;
        return -1;
    }

    // 3. Create a new single-channel (grayscale) image for the output
    //    It has the same dimensions as the input, but only 1 channel (CV_8UC1)
    cv::Mat output_channel_image(input_image.rows, input_image.cols, CV_8UC1);

    // 4. Loop pixel by pixel (as required by the lab)
    for (int row = 0; row < input_image.rows; ++row) {
        for (int col = 0; col < input_image.cols; ++col) {
            
            // Get the BGR pixel value from the input image
            // cv::Vec3b is a vector of 3 unsigned chars (bytes)
            cv::Vec3b bgr_pixel = input_image.at<cv::Vec3b>(row, col);

            // Extract the single channel value
            // bgr_pixel[0] = Blue
            // bgr_pixel[1] = Green
            // bgr_pixel[2] = Red
            uchar channel_value = bgr_pixel[channel_to_extract];

            // Set the pixel value in our new single-channel image
            output_channel_image.at<uchar>(row, col) = channel_value;
        }
    }

    // 5. Write the resulting image to a file
    try {
        cv::imwrite(output_filename, output_channel_image);
        std::cout << "Successfully extracted channel " << channel_to_extract
                  << " to " << output_filename << std::endl;
    } catch (const cv::Exception& ex) {
        std::cerr << "Error writing output image: " << ex.what() << std::endl;
        return -1;
    }

    return 0;
}
