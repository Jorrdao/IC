#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <vector>
#include <cstdint>
#include <cstddef> 

// Classe para escrever bits num buffer
class BitStreamWriter {
private:
    std::vector<uint8_t> buffer;
    uint8_t current_byte;
    int bit_position;

public:
    BitStreamWriter();
    
    void writeBit(int bit);
    void writeBits(unsigned int value, int n_bits);
    void flush();
    const std::vector<uint8_t>& getBuffer();
    void clear();
    size_t size();
};

// Classe para ler bits de um buffer
class BitStreamReader {
private:
    const std::vector<uint8_t>& buffer;
    size_t byte_index;
    int bit_position;

public:
    BitStreamReader(const std::vector<uint8_t>& buf);
    
    int readBit();
    unsigned int readBits(int n_bits);
    bool endOfStream() const;
    void reset();
    size_t position() const;
};

#endif // BITSTREAM_H