#include "bitstream.h"

// ============================================================================
// BitStreamWriter Implementation
// ============================================================================

BitStreamWriter::BitStreamWriter() : current_byte(0), bit_position(0) {}

void BitStreamWriter::writeBit(int bit) {
    if (bit) {
        current_byte |= (1 << (7 - bit_position));
    }
    bit_position++;
    
    if (bit_position == 8) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_position = 0;
    }
}

void BitStreamWriter::writeBits(unsigned int value, int n_bits) {
    for (int i = n_bits - 1; i >= 0; i--) {
        writeBit((value >> i) & 1);
    }
}

void BitStreamWriter::flush() {
    if (bit_position > 0) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_position = 0;
    }
}

const std::vector<uint8_t>& BitStreamWriter::getBuffer() {
    flush();  
    return buffer;
}

void BitStreamWriter::clear() {
    buffer.clear();
    current_byte = 0;
    bit_position = 0;
}

size_t BitStreamWriter::size() {
    flush();  
    return buffer.size();
}

// ============================================================================
// BitStreamReader Implementation
// ============================================================================

BitStreamReader::BitStreamReader(const std::vector<uint8_t>& buf)
    : buffer(buf), byte_index(0), bit_position(0) {}

int BitStreamReader::readBit() {
    if (byte_index >= buffer.size()) {
        return -1;  // End of stream
    }

    int bit = (buffer[byte_index] >> (7 - bit_position)) & 1;
    bit_position++;

    if (bit_position == 8) {
        byte_index++;
        bit_position = 0;
    }

    return bit;
}

unsigned int BitStreamReader::readBits(int n_bits) {
    unsigned int value = 0;
    for (int i = 0; i < n_bits; i++) {
        int bit = readBit();
        if (bit == -1) {
            // End of stream - retorna o que conseguiu ler
            return value;
        }
        value = (value << 1) | bit;
    }
    return value;
}

bool BitStreamReader::endOfStream() const {
    return byte_index >= buffer.size();
}

void BitStreamReader::reset() {
    byte_index = 0;
    bit_position = 0;
}

size_t BitStreamReader::position() const {
    return byte_index * 8 + bit_position;
}