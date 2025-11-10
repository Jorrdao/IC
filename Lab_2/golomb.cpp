#include "golomb.h"

GolombCoder::GolombCoder(int m_value) : m(m_value) {
    if (m <= 0) {
        throw std::invalid_argument("Golomb parameter 'm' must be greater than 0.");
    }

    if (m == 1) {
        // Special case: Unary coding
        b = 0; // Not really used
        t = 0; // Not really used
    } else {
        // Pre-calculate b and t for general Golomb coding
        b = (int)std::ceil(std::log2((double)m));
        t = (1 << b) - m; // (2^b) - m
    }
}

void GolombCoder::encode(BitStreamWriter& writer, int n, SignMode mode) {
    unsigned int mapped_n;

    if (mode == SignMode::INTERLEAVING) {
        // Map 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4, ...
        mapped_n = (n >= 0) ? (2 * n) : (2 * std::abs(n) - 1);
    } else { // SIGN_MAGNITUDE
        // Write a sign bit (0 for positive/zero, 1 for negative)
        int sign_bit = (n < 0) ? 1 : 0;
        writer.writeBit(sign_bit);
        mapped_n = std::abs(n);
    }

    encodeUnsigned(writer, mapped_n);
}

void GolombCoder::encodeUnsigned(BitStreamWriter& writer, unsigned int n) {
    // 1. Calculate quotient (q) and remainder (r)
    unsigned int q = n / m;
    unsigned int r = n % m;

    // 2. Encode q in unary (q ones, followed by a zero)
    for (unsigned int i = 0; i < q; ++i) {
        writer.writeBit(1);
    }
    writer.writeBit(0);

    // 3. Encode r (if m > 1)
    if (m == 1) {
        return; // No remainder to encode for unary
    }

    // General Golomb code for remainder
    if (r < t) {
        // r < (2^b - m), encode r using (b-1) bits
        writer.writeBits(r, b - 1);
    } else {
        // r >= (2^b - m), encode (r + t) using b bits
        writer.writeBits(r + t, b);
    }
}

int GolombCoder::decode(BitStreamReader& reader, SignMode mode) {
    int sign_bit = 0;
    if (mode == SignMode::SIGN_MAGNITUDE) {
        sign_bit = reader.readBit();
        if (sign_bit == -1) return 0; // End of stream
    }

    unsigned int mapped_n = decodeUnsigned(reader);

    if (mode == SignMode::INTERLEAVING) {
        // Unmap: 0 -> 0, 1 -> -1, 2 -> 1, 3 -> -2, 4 -> 2, ...
        if (mapped_n % 2 == 0) {
            return (int)(mapped_n / 2); // Positive or zero
        } else {
            return -(int)((mapped_n + 1) / 2); // Negative
        }
    } else { // SIGN_MAGNITUDE
        return (sign_bit == 1) ? -(int)(mapped_n) : (int)(mapped_n);
    }
}

unsigned int GolombCoder::decodeUnsigned(BitStreamReader& reader) {
    // 1. Decode q from unary
    unsigned int q = 0;
    while (reader.readBit() == 1) {
        q++;
    }
    // The last bit read was 0 (the unary separator)

    // 2. Decode r (if m > 1)
    if (m == 1) {
        return q; // Unary
    }

    unsigned int r;
    
    // Read the first (b-1) bits
    unsigned int r_part1 = reader.readBits(b - 1);

    if (r_part1 < t) {
        // Remainder was encoded with (b-1) bits
        r = r_part1;
    } else {
        // Remainder was encoded with b bits. Read the last bit.
        int r_part2_bit = reader.readBit();
        unsigned int combined_r = (r_part1 << 1) | r_part2_bit;
        r = combined_r - t;
    }

    return q * m + r;
}