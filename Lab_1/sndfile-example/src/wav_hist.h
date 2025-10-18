#include <vector>
#include <map>
#include <iostream>
#include <sndfile.hh>

class WAVHist {
  private:
    std::vector<std::map<short, size_t>> counts;
    std::vector<std::map<short, size_t>> mid_values;
    std::vector<std::map<short, size_t>> side_values;

  public:
    WAVHist(const SndfileHandle& sfh) {
        counts.resize(sfh.channels());
        // Initialize mid and side vectors
        mid_values.resize(1);
        side_values.resize(1);
    }

    void update(const std::vector<short>& samples) {
        size_t n { };
        for(auto s : samples)
            counts[n++ % counts.size()][s]++;
    }

    void update_mid(const std::vector<short>& samples) {
        // Check if audio is stereo
        if(counts.size() != 2) {
            std::cerr << "Mid processing requires stereo audio (2 channels)\n";
            return;
        }
        for(long unsigned int i = 0; i < samples.size()/2; i++) {
            mid_values[0][(samples[2*i] + samples[2*i+1]) / 2]++;
        }
    }

    void update_side(const std::vector<short>& samples) {
        // Check if audio is stereo
        if(counts.size() != 2) {
            std::cerr << "Side processing requires stereo audio (2 channels)\n";
            return;
        }
        for(long unsigned int i = 0; i < samples.size()/2; i++) {
            side_values[0][(samples[2*i] - samples[2*i+1]) / 2]++;
        }
    }

    void dump(const size_t channel) const {
        for(auto [value, counter] : counts[channel])
            std::cout << value << '\t' << counter << '\n';
    }

    void mid_dump() const {
        if(mid_values[0].empty()) {
            std::cerr << "No mid channel data available\n";
            return;
        }
        for(auto [value, counter] : mid_values[0])
            std::cout << value << '\t' << counter << '\n';
    }

    void side_dump() const {
        if(side_values[0].empty()) {
            std::cerr << "No side channel data available\n";
            return;
        }
        for(auto [value, counter] : side_values[0])
            std::cout << value << '\t' << counter << '\n';
    }
};
