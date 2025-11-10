#include <iostream>
#include <string>
#include <vector>
#include <algorithm> // For std::min and std::max
#include <opencv2/opencv.hpp>

// --- DEFINE YOUR FOLDER PATHS HERE ---
// Note the trailing slash '/'
const std::string INPUT_DIR = "imagens_PPM/";
const std::string OUTPUT_DIR = "Result_Images/";

/**
 * @brief Prints the usage message for the program.
 * @param program_name The name of the executable (argv[0]).
 */
void printUsage(const std::string& program_name) {
    std::cerr << "Usage: " << program_name << " <input> <output> <mode> [options]" << std::endl;
    std::cerr << "\nModes & Options:\n";
    std::cerr << "  negative                  - No options needed.\n";
    std::cerr << "  mirror <h | v>            - 'h' for horizontal, 'v' for vertical.\n";
    std::cerr << "  rotate <90 | 180 | 270>   - Degrees to rotate clockwise.\n";
    std::cerr << "  intensity <value>         - Integer value (e.g., 50 or -50).\n";
    std::cerr << "\nExample:\n";
    std::cerr << "  " << program_name << " girl.ppm girl_negative.png negative" << std::endl;
    std::cerr << "  " << program_name << " girl.ppm girl_mirrored.png mirror h" << std::endl;
    std::cerr << "  " << program_name << " girl.ppm girl_rotated.png rotate 90" << std::endl;
    std::cerr << "  " << program_name << " girl.ppm girl_brighter.png intensity 50" << std::endl;
}

/**
 * @brief Clamps an integer value to the valid uchar range [0, 255].
 * @param value The value to clamp.
 * @return A uchar in the range [0, 255].
 */
uchar clamp(int value) {
    return (uchar)std::max(0, std::min(255, value));
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage(argv[0]);
        return -1;
    }

    // --- 1. Parse Basic Arguments ---
    std::string input_filename = INPUT_DIR + argv[1];
    std::string output_filename = OUTPUT_DIR + argv[2];
    std::string mode = argv[3];

    // --- 2. Read Input Image ---
    cv::Mat input_image = cv::imread(input_filename);
    if (input_image.empty()) {
        std::cerr << "Error: Could not read input image " << input_filename << std::endl;
        return -1;
    }

    cv::Mat output_image;
    int channels = input_image.channels();

    try {
        // --- 3. Select Operation Mode ---

        if (mode == "negative") {
            if (argc != 4) {
                printUsage(argv[0]);
                return -1;
            }
            output_image = cv::Mat(input_image.size(), input_image.type());

            for (int r = 0; r < input_image.rows; ++r) {
                for (int c = 0; c < input_image.cols; ++c) {
                    if (channels == 3) { // BGR Image
                        cv::Vec3b pixel = input_image.at<cv::Vec3b>(r, c);
                        pixel[0] = 255 - pixel[0]; // Blue
                        pixel[1] = 255 - pixel[1]; // Green
                        pixel[2] = 255 - pixel[2]; // Red
                        output_image.at<cv::Vec3b>(r, c) = pixel;
                    } else if (channels == 1) { // Grayscale Image
                        uchar pixel = input_image.at<uchar>(r, c);
                        output_image.at<uchar>(r, c) = 255 - pixel;
                    }
                }
            }
        } 
        else if (mode == "mirror") {
            if (argc != 5) {
                std::cerr << "Error: 'mirror' mode requires a direction (h or v)." << std::endl;
                printUsage(argv[0]);
                return -1;
            }
            std::string direction = argv[4];
            output_image = cv::Mat(input_image.size(), input_image.type());

            for (int r = 0; r < input_image.rows; ++r) {
                for (int c = 0; c < input_image.cols; ++c) {
                    if (direction == "h") {
                        int mirrored_c = input_image.cols - 1 - c;
                        if (channels == 3)
                            output_image.at<cv::Vec3b>(r, c) = input_image.at<cv::Vec3b>(r, mirrored_c);
                        else
                            output_image.at<uchar>(r, c) = input_image.at<uchar>(r, mirrored_c);
                    } else if (direction == "v") {
                        int mirrored_r = input_image.rows - 1 - r;
                        if (channels == 3)
                            output_image.at<cv::Vec3b>(r, c) = input_image.at<cv::Vec3b>(mirrored_r, c);
                        else
                            output_image.at<uchar>(r, c) = input_image.at<uchar>(mirrored_r, c);
                    } else {
                        std::cerr << "Error: Invalid mirror direction. Use 'h' or 'v'." << std::endl;
                        return -1;
                    }
                }
            }
        } 
        else if (mode == "rotate") {
            if (argc != 5) {
                std::cerr << "Error: 'rotate' mode requires an angle (90, 180, or 270)." << std::endl;
                printUsage(argv[0]);
                return -1;
            }
            int angle = std::stoi(argv[4]);

            if (angle == 180) {
                output_image = cv::Mat(input_image.size(), input_image.type());
                for (int r = 0; r < output_image.rows; ++r) {
                    for (int c = 0; c < output_image.cols; ++c) {
                        if (channels == 3)
                            output_image.at<cv::Vec3b>(r, c) = input_image.at<cv::Vec3b>(input_image.rows - 1 - r, input_image.cols - 1 - c);
                        else
                            output_image.at<uchar>(r, c) = input_image.at<uchar>(input_image.rows - 1 - r, input_image.cols - 1 - c);
                    }
                }
            } else if (angle == 90) {
                output_image = cv::Mat(input_image.cols, input_image.rows, input_image.type()); // Swapped dimensions
                for (int r = 0; r < output_image.rows; ++r) { // Loop over *output* rows
                    for (int c = 0; c < output_image.cols; ++c) { // Loop over *output* cols
                        if (channels == 3)
                            output_image.at<cv::Vec3b>(r, c) = input_image.at<cv::Vec3b>(input_image.rows - 1 - c, r);
                        else
                            output_image.at<uchar>(r, c) = input_image.at<uchar>(input_image.rows - 1 - c, r);
                    }
                }
            } else if (angle == 270) {
                output_image = cv::Mat(input_image.cols, input_image.rows, input_image.type()); // Swapped dimensions
                for (int r = 0; r < output_image.rows; ++r) {
                    for (int c = 0; c < output_image.cols; ++c) {
                        if (channels == 3)
                            output_image.at<cv::Vec3b>(r, c) = input_image.at<cv::Vec3b>(c, input_image.cols - 1 - r);
                        else
                            output_image.at<uchar>(r, c) = input_image.at<uchar>(c, input_image.cols - 1 - r);
                    }
                }
            } else {
                std::cerr << "Error: Invalid angle. Use 90, 180, or 270." << std::endl;
                return -1;
            }
        } 
        else if (mode == "intensity") {
            if (argc != 5) {
                std::cerr << "Error: 'intensity' mode requires a value (e.g., 50)." << std::endl;
                printUsage(argv[0]);
                return -1;
            }
            int value = std::stoi(argv[4]);
            output_image = cv::Mat(input_image.size(), input_image.type());

            for (int r = 0; r < input_image.rows; ++r) {
                for (int c = 0; c < input_image.cols; ++c) {
                    if (channels == 3) {
                        cv::Vec3b pixel = input_image.at<cv::Vec3b>(r, c);
                        pixel[0] = clamp((int)pixel[0] + value); // Blue
                        pixel[1] = clamp((int)pixel[1] + value); // Green
                        pixel[2] = clamp((int)pixel[2] + value); // Red
                        output_image.at<cv::Vec3b>(r, c) = pixel;
                    } else if (channels == 1) {
                        uchar pixel = input_image.at<uchar>(r, c);
                        output_image.at<uchar>(r, c) = clamp((int)pixel + value);
                    }
                }
            }
        } 
        else {
            std::cerr << "Error: Unknown mode '" << mode << "'." << std::endl;
            printUsage(argv[0]);
            return -1;
        }

        // --- 4. Write Output Image ---
        if (output_image.empty()) {
            std::cerr << "Error: Output image was not created. Check mode and options." << std::endl;
            return -1;
        }

        cv::imwrite(output_filename, output_image);
        std::cout << "Successfully applied '" << mode << "' and saved to " << output_filename << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        std::cerr << "This often happens if you provide a non-integer for angle or intensity." << std::endl;
        return -1;
    }

    return 0;
}
