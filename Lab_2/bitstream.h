#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <vector>
#include <cstdint> // For uint8_t
#include <iostream>

/**
 * @class BitStreamWriter
 * @brief Manages writing individual bits or bit sequences to a byte buffer.
 */
class BitStreamWriter {
public:
    /**
     * @brief Default constructor.
     */
    BitStreamWriter();

    /**
     * @brief Writes a single bit (0 or 1).
     * @param bit The bit to write.
     */
    void writeBit(int bit);

    /**
     * @brief Writes a sequence of bits from an unsigned integer.
     * @param value The integer containing the bits to write.
     * @param num_bits The number of bits to write (from the least significant side).
     */
    void writeBits(unsigned int value, int num_bits);

    /**
     * @brief Flushes any remaining bits in the current byte to the buffer.
     * Call this when you are finished writing.
     */
    void flush();

    /**
     * @brief Gets the internal byte buffer.
     * @return A const reference to the byte vector.
     */
    const std::vector<uint8_t>& getBuffer() const;

private:
    std::vector<uint8_t> buffer;
    uint8_t current_byte;
    int bit_position; // 0-7, how many bits are in current_byte
};

/**
 * @class BitStreamReader
 * @brief Manages reading individual bits or bit sequences from a byte buffer.
 */
class BitStreamReader {
public:
    /**
     * @brief Constructor that takes the buffer to read from.
     * @param buffer The byte buffer to read.
     */
    BitStreamReader(const std::vector<uint8_t>& buffer);

    /**
     * @brief Reads a single bit.
     * @return The bit (0 or 1), or -1 if the stream is empty.
     */
    int readBit();

    /**
     * @brief Reads a sequence of bits into an unsigned integer.
     * @param num_bits The number of bits to read.
     * @return The resulting unsigned integer, or 0 if stream ended prematurely (error).
     */
    unsigned int readBits(int num_bits);

    /**
     * @brief Checks if the stream has more bits to read.
     * @return True if more bits are available, false otherwise.
     */
    bool hasMoreBits() const;

private:
    const std::vector<uint8_t>& buffer;
    size_t byte_index;   // Current byte we are reading from
    int bit_position; // 0-7, which bit to read next
};

#endif // BITSTREAM_H