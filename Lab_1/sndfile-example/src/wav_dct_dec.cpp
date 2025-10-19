#include <iostream>
#include <vector>
#include <cmath>
#include <fftw3.h>
#include <sndfile.hh>
#include <fstream>
#include <string>
#include <cstdint>

#include "bit_stream.h"
#include "byte_stream.h"

using namespace std;

int main(int argc, char *argv[]) {

    // Default values (will be overwritten by metadata)
	bool verbose { false };

	if (argc < 3) {
        cerr << "Usage: wav_dct_dec [ -v (verbose) ]\n"; 
        cerr << "                   encFileIn wavFileOut\n";
        return 1;
    }

    for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-v") {
			verbose = true;
			break;
		}

    // --- Input BitStream setup ---
    fstream fsIn { argv[argc-2], ios::in | ios::binary };
    if(!fsIn.is_open()) {
        cerr << "Error opening input bitstream file " << argv[argc-2] << endl;
        return 1;
    }
    BitStream bsIn { fsIn, STREAM_READ };

    // --- 1. Read Metadata from BitStream ---
    int sampleRate = static_cast<int>(bsIn.read_n_bits(32));
    // Read block size (16 bits)
    size_t bs = static_cast<size_t>(bsIn.read_n_bits(16));
    // Read number of *kept* coefficients (16 bits)
    size_t nDctCoeffsPerBlock = static_cast<size_t>(bsIn.read_n_bits(16));
    // Read number of bits used for quantization (8 bits)
    int N_BITS_QUANT = static_cast<int>(bsIn.read_n_bits(8));
    // Read total number of frames (32 bits)
    sf_count_t nFrames = static_cast<sf_count_t>(bsIn.read_n_bits(32));

    const size_t nChannels = 1;

    if (verbose) {
        cerr << "--- Encoded File Parameters ---\n";
        cerr << "Sample Rate: " << sampleRate << endl;
        cerr << "Block Size (bs): " << bs << endl;
        cerr << "Coefficients Kept: " << nDctCoeffsPerBlock << endl;
        cerr << "Quantization Bits: " << N_BITS_QUANT << endl;
        cerr << "Total Frames: " << nFrames << endl;
        cerr << "--------------------------------\n";
    }

    if (sampleRate <= 0) {
        cerr << "Error: Invalid sample rate read from header.\n";
        return 1;
    }
    
    // --- 4. IDCT Setup and Processing ---

    size_t nBlocks = static_cast<size_t>(ceil(static_cast<double>(nFrames) / bs));

    // Vector for holding IDCT computations for the current block (same size as block size)
    vector<double> x(bs);

    // Inverse DCT plan (FFTW_REDFT01 is IDCT-II, the inverse of DCT-II)
    fftw_plan plan_id = fftw_plan_r2r_1d(bs, x.data(), x.data(), FFTW_REDFT01, FFTW_ESTIMATE);

    // Vector to store the reconstructed audio samples
    vector<short> samples(nBlocks * bs * nChannels);

    if(verbose) cerr << "Decoding " << nBlocks << " blocks...\n";

    for(size_t n = 0 ; n < nBlocks ; n++) {
        for(size_t c = 0 ; c < nChannels ; c++) { // nChannels is 1 (mono)

            // --- De-quantization and IDCT Input Setup ---

            // 1. Clear the DCT vector. Only the first nDctCoeffsPerBlock will be populated.
            for (size_t k = 0; k < bs; k++)
                x[k] = 0.0;

            // 2. Read quantized coefficients and place them in the vector 'x'
            for(size_t k = 0 ; k < nDctCoeffsPerBlock ; k++) {
                // Read the N_BITS_QUANT value
                uint64_t raw_val = bsIn.read_n_bits(N_BITS_QUANT);

                // Reconstruct the signed integer value (de-quantization)
                // Assuming N_BITS_QUANT <= 32 based on encoder.
                // Cast to int32_t to recover the signed representation
                int32_t q_val = static_cast<int32_t>(raw_val);

                // Cast to double for the IDCT computation
                x[k] = static_cast<double>(q_val);
            }

            // 3. Execute IDCT
            fftw_execute(plan_id);

            // 4. Scaling and storing the reconstructed time-domain samples
            // The unnormalized DCT-II/IDCT-II pair in FFTW results in a factor of 2*bs.
            // We must divide by this factor to restore the original magnitude.
            double scale = 2.0 * bs;

            for(size_t k = 0 ; k < bs ; k++) {
                // Scale the IDCT output and cast to short for WAV format
                long scaled_sample = lround(x[k] / scale);

                // Clip to the valid range for a 16-bit short [-32768, 32767]
                if (scaled_sample > 32767) scaled_sample = 32767;
                if (scaled_sample < -32768) scaled_sample = -32768;

                samples[(n * bs + k) * nChannels + c] = static_cast<short>(scaled_sample);
            }
        }
    }


    // --- 5. Output WAV File Setup and Writing ---

    // Determine the number of frames to write (original nFrames, ignoring zero-padding)
    sf_count_t nFramesToWrite = nFrames;
    
    int sf_format = SF_FORMAT_WAV | SF_FORMAT_PCM_16; // 16-bit PCM WAV

    SndfileHandle sfhOut {
        argv[argc-1],
        SFM_WRITE,
        sf_format,
        (int)nChannels,
        sampleRate
    };

    if(sfhOut.error()) {
        cerr << "Error: failed to open WAV file " << argv[argc-1] << " for writing: " << sfhOut.strError() << endl;
        return 1;
    }

    if (verbose) {
        cerr << "Writing " << nFramesToWrite << " frames to " << argv[argc-1] << "...\n";
    }

    // Write the reconstructed samples, limited to the original number of frames
    sfhOut.writef(samples.data(), nFramesToWrite);

    // --- 6. Cleanup ---
    fftw_destroy_plan(plan_id);
    fsIn.close();

    if(verbose) cerr << "Decoding complete.\n";

    return 0;
}