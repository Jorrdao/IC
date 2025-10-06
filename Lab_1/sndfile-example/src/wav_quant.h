#ifndef WAVQUANT_H
#define WAVQUANT_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>


class WAVQuant {
    private:
        std::vector<short> num_samples;

    public:
        WAVQuant() {
            quant_samples.resize(0);
        }

        void quant(const std::vector<short>& samples, size_t num_bits) {
            for (auto sample : samples) {
                // right shift to remove the least significant bits
                sample = sample >> num_bits;
                // shift left to fill the least significant bits with 0s
                short aux = sample << num_bits;
                //save the sample in the vector 
                num_samples.insert(num_samples.end(), aux);
            }
        }

        void toFile(SndfileHandle sfhOut) const {
            //print quant_samples size
            sfhOut.write(num_samples.data(), num_samples.size());
        }
};

#endif