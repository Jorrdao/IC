#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.hh>

using namespace std;

/**
 * @brief Reads a stereo or mono WAV file, downmixes it to mono, and writes a new mono WAV file.
 * * Usage: wav_mono [ -v (verbose) ] wavFileIn wavFileOut
 */
int main(int argc, char *argv[]) {

    bool verbose { false };

	if(argc < 3) {
		cerr << "Usage: wav_mono [ -v (verbose) ] wavFileIn wavFileOut\n";
		return 1;
	}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-v") {
			verbose = true;
			break;
		}
    
    // File paths are the last two arguments
    string wavFileIn = argv[argc-2];
    string wavFileOut = argv[argc-1];

	SndfileHandle sfhIn { wavFileIn };

	if(sfhIn.error()) {
		cerr << "Error: failed to open WAV file " << wavFileIn << " for reading: " << sfhIn.strError() << endl;
		return 1;
	}

    int sampleRate = sfhIn.samplerate();
    int nChannelsIn = sfhIn.channels(); 
    sf_count_t nFrames = sfhIn.frames();
    
    if (nChannelsIn > 2) {
        cerr << "Error: Only mono (1) or stereo (2) files supported for downmixing.\n";
        return 1;
    }
    
    // Read all original samples
    vector<short> original_samples(nChannelsIn * nFrames);
    sfhIn.readf(original_samples.data(), nFrames);

    // Create a mono buffer to write
    vector<short> mono_samples(nFrames); 
    
    // Downmix: Calculate the average (MID channel)
    for (size_t i = 0; i < nFrames; ++i) {
        short mono_sample = 0;
        // If stereo, sum L and R. If mono, only L is present.
        for (int c = 0; c < nChannelsIn; ++c) {
            mono_sample += original_samples[i * nChannelsIn + c];
        }
        // Division by nChannelsIn performs the average for downmixing
        mono_samples[i] = nChannelsIn > 1 ? mono_sample / nChannelsIn : mono_sample;
    }

    // --- Output WAV File Setup and Writing ---
    
    const int nChannelsOut = 1; // Always output mono
    int sf_format = SF_FORMAT_WAV | SF_FORMAT_PCM_16; // 16-bit PCM WAV

    SndfileHandle sfhOut {
        wavFileOut,
        SFM_WRITE,
        sf_format,
        nChannelsOut, // Always 1 channel
        sampleRate
    };

    if(sfhOut.error()) {
        cerr << "Error: failed to open WAV file " << wavFileOut << " for writing: " << sfhOut.strError() << endl;
        return 1;
    }

    if (verbose) {
        cerr << "Input channels: " << nChannelsIn << ". Writing " << nFrames << " mono frames to " << wavFileOut << endl;
    }

    // Write all frames of the reconstructed audio
    if (sfhOut.writef(mono_samples.data(), nFrames) != nFrames) {
        cerr << "Error: did not write all frames to output WAV file.\n";
        return 1;
    }

    if(verbose) cerr << "Downmixing complete.\n";

	return 0;
}
