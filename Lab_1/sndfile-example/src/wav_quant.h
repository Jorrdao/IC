#ifndef WAVQUANT_H
#define WAVQUANT_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>

class WAVQuant {
public:
    WAVQuant() {}

    void quant(std::vector<short>& samples, size_t num_bits) {
        for (auto& sample : samples) {
            sample = (sample >> num_bits) << num_bits;
        }
    }
};

#endif