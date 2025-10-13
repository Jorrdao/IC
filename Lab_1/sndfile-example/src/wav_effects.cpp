#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm> // For std::reverse
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " <input file> <output_file> <effect> [params...]\n";
        return 1;
    }

    SndfileHandle sfhIn { argv[1] };
    if(sfhIn.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }

    if((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV || (sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: Input file must be a 16-bit PCM WAV file.\n";
        return 1;
    }

    SndfileHandle sfhOut { argv[2], SFM_WRITE, sfhIn.format(), sfhIn.channels(), sfhIn.samplerate() };
    if(sfhOut.error()) {
        cerr << "Error: invalid output file\n";
        return 1;
    }

    string wanted_effect = argv[3];
    float gain = 0.5f;
    int delay_samples = 0;
    float freq = 4.0f;
    int num_channels = sfhIn.channels();

    // --- Argument Parsing ---
    if (wanted_effect == "single_echo" || wanted_effect == "multiple_echo") {
        if (argc != 6) {
            cerr << "Usage: " << argv[0] << " <in> <out> " << wanted_effect << " <delay_ms> <gain>\n";
            return 1;
        }
        try {
            // Convert delay from milliseconds to samples
            delay_samples = (stoi(argv[4]) / 1000.0) * sfhIn.samplerate() * num_channels;
            gain = stof(argv[5]);
        } catch(const exception& err) {
            cerr << "Error: invalid delay or gain\n";
            return 1;
        }
    } else if (wanted_effect == "amplitude_modulation") {
        if (argc != 5) {
            cerr << "Usage: " << argv[0] << " <in> <out> amplitude_modulation <freq_hz>\n";
            return 1;
        }
        try {
            freq = stof(argv[4]);
        } catch(const exception& err) {
            cerr << "Error: invalid frequency\n";
            return 1;
        }
    } else if (wanted_effect == "reverse") {
        if (argc != 4) {
            cerr << "Usage: " << argv[0] << " <in> <out> reverse\n";
            return 1;
        }
    } else {
        cerr << "Error: invalid effect '" << wanted_effect << "'\n";
        return 1;
    }


    // --- Effect Processing ---
    size_t nFrames;
    vector<short> buffer(FRAMES_BUFFER_SIZE * num_channels);

    if (wanted_effect == "single_echo" || wanted_effect == "multiple_echo") {
        vector<short> delay_buffer(delay_samples, 0);
        size_t delay_cursor = 0;

        while((nFrames = sfhIn.readf(buffer.data(), FRAMES_BUFFER_SIZE))) {
            size_t nSamples = nFrames * num_channels;
            for(size_t i = 0; i < nSamples; ++i) {
                short delayed_sample = delay_buffer[delay_cursor];
                short current_sample = buffer[i];

                // Add the echo
                buffer[i] = current_sample + gain * delayed_sample;

                // Update the delay buffer for the next iteration
                short feedback_sample = (wanted_effect == "multiple_echo") ? buffer[i] : current_sample;
                delay_buffer[delay_cursor] = feedback_sample;
                
                delay_cursor = (delay_cursor + 1) % delay_samples;
            }
            sfhOut.writef(buffer.data(), nFrames);
        }
    } else if (wanted_effect == "amplitude_modulation") {
        long long total_samples_processed = 0;
        double sample_rate = sfhIn.samplerate();

        while((nFrames = sfhIn.readf(buffer.data(), FRAMES_BUFFER_SIZE))) {
            size_t nSamples = nFrames * num_channels;
            for(size_t i = 0; i < nSamples; ++i) {
                // Use a global sample counter for continuous phase
                double time = (double)(total_samples_processed + i / num_channels) / sample_rate;
                buffer[i] *= cos(2 * M_PI * freq * time);
            }
            sfhOut.writef(buffer.data(), nFrames);
            total_samples_processed += nSamples;
        }
    } else if (wanted_effect == "reverse") {
        // For reverse, we must read the whole file first.
        sfhIn.seek(0, SEEK_SET); // Rewind file handle
        vector<short> all_samples(sfhIn.frames() * num_channels);
        sfhIn.readf(all_samples.data(), sfhIn.frames());

        // Reverse the entire vector of samples
        std::reverse(all_samples.begin(), all_samples.end());

        // If stereo, swap left and right channels back
        if(num_channels > 1) {
            for(size_t i = 0; i < all_samples.size(); i += num_channels) {
                std::reverse(all_samples.begin() + i, all_samples.begin() + i + num_channels);
            }
        }
        
        sfhOut.writef(all_samples.data(), sfhIn.frames());
    }

    cout << "Effect '" << wanted_effect << "' applied successfully." << endl;

    return 0;
}