#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <cmath>
#include <iomanip>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {

	if(argc < 3) {
		cerr << "Usage: " << argv[0] << " <input file1> <input file2>\n";
		return 1;
	}

	SndfileHandle sfhIn1 { argv[argc-2] };
	if(sfhIn1.error()) {
		cerr << "Error: invalid input file1\n";
		return 1;
    }

    SndfileHandle sfhIn2 { argv[argc-1] };
	if(sfhIn2.error()) {
		cerr << "Error: invalid input file2\n";
		return 1;
    }

    // --- Format and Property Validation ---
	if((sfhIn1.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
       (sfhIn2.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
		cerr << "Error: Both files must be in WAV format\n";
		return 1;
	}
	if((sfhIn1.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16 ||
       (sfhIn2.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
		cerr << "Error: Both files must be in PCM_16 format\n";
		return 1;
	}

    if(sfhIn1.frames() != sfhIn2.frames()) {
        cerr << "Error: files have different number of frames\n";
        return 1;
    }

    if(sfhIn1.channels() != sfhIn2.channels()) {
        cerr << "Error: files have different number of channels\n";
        return 1;
    }

    // --- Initialization ---
    int num_channels = sfhIn1.channels();
    vector<short> samples_f1(FRAMES_BUFFER_SIZE * num_channels);
    vector<short> samples_f2(FRAMES_BUFFER_SIZE * num_channels);

    // Use vectors to hold per-channel results
    vector<double> energy_signal(num_channels, 0.0);
    vector<double> energy_noise(num_channels, 0.0);
    vector<double> max_error(num_channels, 0.0);

    size_t num_Frames;

    // --- Processing Loop ---
    while((num_Frames = sfhIn1.readf(samples_f1.data(), FRAMES_BUFFER_SIZE))) {
        sfhIn2.readf(samples_f2.data(), FRAMES_BUFFER_SIZE);

        size_t num_Samples = num_Frames * num_channels;

        for (size_t i = 0; i < num_Samples; i++) {
            int channel = i % num_channels;

            // Use long long to prevent overflow during multiplication
            long long s1 = samples_f1[i];
            long long diff = s1 - samples_f2[i];
            long long abs_diff = abs(diff);

            // Correctly square the values
            energy_signal[channel] += s1 * s1;
            energy_noise[channel] += diff * diff;

            if (abs_diff > max_error[channel]) {
                max_error[channel] = abs_diff;
            }
        }
    }

    // --- Output Results ---
    cout << fixed << setprecision(4);
    cout << "------------------------------------------------------------------" << endl;
    cout << left << setw(12) << "Channel"
         << left << setw(20) << "SNR (dB)"
         << left << setw(20) << "MSE"
         << left << setw(20) << "Max Abs Error (L∞)" << endl;
    cout << "------------------------------------------------------------------" << endl;

    double total_energy_signal = 0.0;
    double total_energy_noise = 0.0;
    double overall_max_error = 0.0;

    for(int c = 0; c < num_channels; c++) {
        double snr = (energy_noise[c] == 0) ? INFINITY : 10 * log10(energy_signal[c] / energy_noise[c]);
        double mse = energy_noise[c] / sfhIn1.frames();

        cout << left << setw(12) << (c + 1)
             << left << setw(20) << snr
             << left << setw(20) << mse
             << left << setw(20) << max_error[c] << endl;

        total_energy_signal += energy_signal[c];
        total_energy_noise += energy_noise[c];
        if(max_error[c] > overall_max_error) {
            overall_max_error = max_error[c];
        }
    }

    // Calculate and print average/combined results
    if (num_channels > 1) {
        double avg_snr = (total_energy_noise == 0) ? INFINITY : 10 * log10(total_energy_signal / total_energy_noise); // SNR for all channels combined
        double avg_mse = total_energy_noise / (sfhIn1.frames() * num_channels); // Average MSE across all channels

        cout << "------------------------------------------------------------------" << endl;
        cout << left << setw(12) << "Average"
             << left << setw(20) << avg_snr
             << left << setw(20) << avg_mse
             << left << setw(20) << overall_max_error << endl;
    }
    cout << "------------------------------------------------------------------" << endl;

    return 0;
}