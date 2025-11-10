#include "bitstream.h"

// --- BitStreamWriter Implementation ---

BitStreamWriter::BitStreamWriter() : current_byte(0), bit_position(0) {}

void BitStreamWriter::writeBit(int bit) {
    if (bit) {
        // Set the bit at the correct position
        // We write from MSB (7) to LSB (0)
        current_byte |= (1 << (7 - bit_position));
    }
    
    bit_position++;

    // If the byte is full, push it to the buffer and reset
    if (bit_position == 8) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_position = 0;
    }
}

void BitStreamWriter::writeBits(unsigned int value, int num_bits) {
    if (num_bits == 0) return;

    for (int i = num_bits - 1; i >= 0; --i) {
        // Get the i-th bit from the value
        int bit = (value >> i) & 1;
        writeBit(bit);
    }
}

void BitStreamWriter::flush() {
    // If there are any bits left in the current_byte, push it
    if (bit_position > 0) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_position = 0;
    }
}

const std::vector<uint8_t>& BitStreamWriter::getBuffer() const {
    return buffer;
}


// --- BitStreamReader Implementation ---

BitStreamReader::BitStreamReader(const std::vector<uint8_t>& buf)
    : buffer(buf), byte_index(0), bit_position(0) {}

int BitStreamReader::readBit() {
    if (!hasMoreBits()) {
        return -1; // End of stream
    }

    // Read the bit from the current byte
    int bit = (buffer[byte_index] >> (7 - bit_position)) & 1;

    bit_position++;

    // Move to the next byte if this one is finished
    if (bit_position == 8) {
        byte_index++;
        bit_position = 0;
    }

    return bit;
}

unsigned int BitStreamReader::readBits(int num_bits) {
    if (num_bits == 0) return 0;

    unsigned int value = 0;
    for (int i = 0; i < num_bits; ++i) {
        int bit = readBit();
        if (bit == -1) {
            // Reached end of stream unexpectedly
            // Returning 0, but in a real-world app, you might throw an error
            return 0; 
        }
        // Add the new bit to the LSB side
        value = (value << 1) | bit;
    }
    return value;
}

bool BitStreamReader::hasMoreBits() const {
     return byte_index < buffer.size();
}