//------------------------------------------------------------------------------
//
// Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
//
// These programs are supplied free of charge for research purposes only,
// and may not be sold or incorporated into any commercial product. There is
// ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
// fit for ANY PURPOSE WHATSOEVER. Use them at your own risk. If you do
// happen to find a bug, or have modifications to suggest, please report
// the same to Armando J. Pinho, ap@ua.pt. The copyright notice above
// and this statement of conditions must remain an integral part of each
// and every copy made of these files.
//
// Armando J. Pinho (ap@ua.pt)
// IEETA / DETI / University of Aveiro
//
#ifndef WAVHIST_H
#define WAVHIST_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>

class WAVHist {
  private:
	std::vector<std::map<short, size_t>> counts;
	std::vector<std::map<short, size_t>> mid_values;
	std::vector<std::map<short, size_t>> side_values;

  public:
	WAVHist(const SndfileHandle& sfh) {
		counts.resize(sfh.channels());
	}

	void update(const std::vector<short>& samples) {
		size_t n { };
		for(auto s : samples)
			counts[n++ % counts.size()][s]++;
	}

	void dump(const size_t channel) const {
		for(auto [value, counter] : counts[channel])
			std::cout << value << '\t' << counter << '\n';
	}

	void update_mid(const std::vector<short>& samples) {
		for(long unsigned int i = 0; i < samples.size()/2; i++)
			mid_values[0][(samples[2*i] + samples[2*i+1]) / 2]++;
	}

	void mid_dump() const {
		for(auto [value, counter] : mid_values[0])
			std::cout << value << '\t' << counter << '\n';
	}

	void update_side(const std::vector<short>& samples) {
		for(long unsigned int i = 0; i < samples.size()/2; i++)
			side_values[0][(samples[2*i] - samples[2*i+1]) / 2]++;
	}

	void side_dump() const {
		for(auto [value, counter] : side_values[0])
			std::cout << value << '\t' << counter << '\n';
	}

};

#endif

