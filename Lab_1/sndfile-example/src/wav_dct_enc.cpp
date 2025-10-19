#include <iostream>
#include <vector>
#include <cmath>
#include <fftw3.h>
#include <sndfile.hh>
#include <fstream>
#include <string>

#include "bit_stream.h"
#include "byte_stream.h"

using namespace std;

// --- Encoder main function ---
int main(int argc, char *argv[]) {

    // --- 1. Argument Parsing ---
    bool verbose { false };
	size_t bs { 1024 };
	double dctFrac { 0.2 };
    int N_BITS_QUANT { 32 };

	if(argc < 3) {
		cerr << "Usage: wav_dct_enc [ -v (verbose) ]\n";
		cerr << "                   [ -bs blockSize (def 1024) ]\n";
		cerr << "                   [ -frac dctFraction (def 0.2) ]\n";
        cerr << "                   [ -qbits quantizationBits (def 32) ]\n";
		cerr << "                   wavFileIn encFileOut\n";
		return 1;
	}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-v") {
			verbose = true;
			break;
		}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-bs") {
			bs = atoi(argv[n+1]);
			break;
		}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-frac") {
			dctFrac = atof(argv[n+1]);
			break;
		}

    for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-qbits") {
			N_BITS_QUANT = atof(argv[n+1]);
            if (N_BITS_QUANT < 1 || N_BITS_QUANT > 64) {
                cerr << "Error: Quantization bits must be between 1 and 64.\n"; return 1;
            }
			break;
		}

	SndfileHandle sfhIn { argv[argc-2] };
    
	if(sfhIn.error()) {
		cerr << "Error: invalid input file\n";
		return 1;
    }

	if((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
		cerr << "Error: file is not in WAV format\n";
		return 1;
	}

	if((sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
		cerr << "Error: file is not in PCM_16 format\n";
		return 1;
	}


	if(verbose) {
		cout << "Input file has:\n";
		cout << '\t' << sfhIn.frames() << " frames\n";
		cout << '\t' << sfhIn.samplerate() << " samples per second\n";
		cout << '\t' << sfhIn.channels() << " channels\n";
	}

	size_t nChannelsIn { static_cast<size_t>(sfhIn.channels()) };
	size_t nFrames { static_cast<size_t>(sfhIn.frames()) };
    size_t sampleRate { static_cast<size_t>(sfhIn.samplerate()) };

    // --- 3. Output Encoded File Setup (BitStream) ---

    // Open std::fstream in binary write mode
    std::fstream fsOut { argv[argc-1], std::ios::out | std::ios::binary };
    if (!fsOut.is_open()) {
        cerr << "Error: failed to create output file " << argv[argc-1] << endl;
        return 1;
    }

    // Create BitStream in write mode (STREAM_WRITE is defined in byte_stream.h)
    BitStream bsOut { fsOut, STREAM_WRITE };

    size_t nDctCoeffsPerBlock = static_cast<size_t>(bs * dctFrac);

    // --- 4. Write Encoder Header (Parameters needed for Decoder) ---
    if(verbose) cerr << "Writing header info to encoded file...\n";

    bsOut.write_n_bits(static_cast<uint64_t>(sampleRate), 32);
    bsOut.write_n_bits(static_cast<uint64_t>(bs), 16);
    bsOut.write_n_bits(static_cast<uint64_t>(nDctCoeffsPerBlock), 16);
    bsOut.write_n_bits(static_cast<uint64_t>(N_BITS_QUANT), 8);
    bsOut.write_n_bits(static_cast<uint64_t>(nFrames), 32);

    // --- 5. DCT Processing and Encoding ---

    // Read all samples from the input file
    vector<short> samples(nFrames);
    sfhIn.readf(samples.data(), nFrames);

    const size_t nChannelsOut = 1;

    size_t nBlocks = static_cast<size_t>(ceil(static_cast<double>(nFrames) / bs));

    // Do zero padding, if necessary
    samples.resize(nBlocks * bs * nChannelsOut);

    // Vector for holding DCT computations for the current block
    vector<double> x(bs);

    // Direct DCT plan (FFTW_REDFT10 is DCT-II, which is a common choice for this)
    fftw_plan plan_d = fftw_plan_r2r_1d(bs, x.data(), x.data(), FFTW_REDFT10, FFTW_ESTIMATE);

    if(verbose) cerr << "Encoding " << nBlocks << " blocks...\n";

    for(size_t n = 0 ; n < nBlocks ; n++) {
        for(size_t c = 0 ; c < nChannelsOut ; c++) { // nChannels is 1 (mono)
            // Copy samples of the current channel/block into the DCT input vector
            for(size_t k = 0 ; k < bs ; k++)
                x[k] = samples[(n * bs + k) * nChannelsOut + c];

            // Execute DCT
            fftw_execute(plan_d);

            // --- 7. Quantization and Writing to BitStream ---

            for(size_t k = 0 ; k < nDctCoeffsPerBlock ; k++) {
                double dct_coeff = x[k];

                long q_val = lround(dct_coeff);

                bsOut.write_n_bits(static_cast<uint64_t>(q_val), N_BITS_QUANT);
            }
        }
    }

    // --- 7. Cleanup ---
    fftw_destroy_plan(plan_d);

    if(verbose) cerr << "Closing BitStream...\n";
    bsOut.close();
    fsOut.close();

    if(verbose) cerr << "Encoding complete.\n";

    return 0;
}