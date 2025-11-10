#ifndef GOLOMBCODER_H
#define GOLOMBCODER_H

#include "bitstream.h"
#include <cmath> // For log2 and ceil
#include <stdexcept> // For exceptions

class GolombCoder {
public:
    /**
     * @brief Enum for selecting the negative number handling strategy.
     */
    enum class SignMode {
        SIGN_MAGNITUDE,
        INTERLEAVING
    };

    /**
     * @brief Constructor for the GolombCoder.
     * @param m The Golomb parameter 'm'. Must be > 0.
     */
    GolombCoder(int m_value);

    /**
     * @brief Encodes a signed integer.
     * @param writer The BitStreamWriter to write bits to.
     * @param n The integer to encode.
     * @param mode The strategy for handling negative numbers.
     */
    void encode(BitStreamWriter& writer, int n, SignMode mode);

    /**
     * @brief Decodes a signed integer.
     * @param reader The BitStreamReader to read bits from.
     * @param mode The strategy used during encoding.
     * @return The decoded integer.
     */
    int decode(BitStreamReader& reader, SignMode mode);

private:
    /**
     * @brief Encodes a non-negative integer using the Golomb code.
     * @param writer The BitStreamWriter.
     * @param n The non-negative integer to encode.
     */
    void encodeUnsigned(BitStreamWriter& writer, unsigned int n);

    /**
     * @brief Decodes a non-negative integer from the Golomb code.
     * @param reader The BitStreamReader.
     * @return The decoded non-negative integer.
     */
    unsigned int decodeUnsigned(BitStreamReader& reader);

    int m;
    // Pre-calculated parameters for encoding/decoding the remainder
    int b;
    int t;
};

#endif // GOLOMBCODER_H