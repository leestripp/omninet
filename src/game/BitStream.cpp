// OmniNet - Bit-level serialization for high-speed game networking
#include "game/BitStream.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace OmniNet::Game {

// ============================================================================
// BitWriter
// ============================================================================

BitWriter::BitWriter() {
    m_buffer.reserve(256);
}

BitWriter::BitWriter(size_t reserveBytes) {
    m_buffer.reserve(reserveBytes);
}

void BitWriter::writeBits(uint64_t value, unsigned numBits) {
    if (numBits == 0 || numBits > 64) return;

    // Mask off anything above numBits (avoid UB shifting by 64)
    if (numBits < 64)
        value &= (uint64_t{1} << numBits) - 1;

    // Grow buffer to hold the new bits
    size_t required = (m_bitCursor + numBits + 7) / 8;
    if (m_buffer.size() < required)
        m_buffer.resize(required, 0);

    // Byte-aligned fast path
    if ((m_bitCursor % 8) == 0 && (numBits % 8) == 0) {
        size_t bytePos = m_bitCursor / 8;
        unsigned byteCount = numBits / 8;
        for (unsigned i = 0; i < byteCount; ++i)
            m_buffer[bytePos + i] = static_cast<uint8_t>(value >> (i * 8));
        m_bitCursor += numBits;
        return;
    }

    // General bit path
    for (unsigned i = 0; i < numBits; ++i) {
        if ((value >> i) & 1)
            m_buffer[m_bitCursor / 8] |= static_cast<uint8_t>(1u << (m_bitCursor % 8));
        ++m_bitCursor;
    }
}

void BitWriter::writeFloat(float f) {
    writeBits(std::bit_cast<uint32_t>(f), 32);
}

void BitWriter::writeDouble(double d) {
    writeBits(std::bit_cast<uint64_t>(d), 64);
}

void BitWriter::writeQuantizedFloat(float value, float min, float max, unsigned numBits) {
    if (numBits == 0 || numBits > 32 || max <= min) return;
    float clamped = std::clamp(value, min, max);
    float t = (clamped - min) / (max - min);
    uint32_t maxInt = (numBits == 32) ? 0xFFFFFFFFu : ((1u << numBits) - 1u);
    auto quantized = static_cast<uint32_t>(t * static_cast<float>(maxInt) + 0.5f);
    writeBits(quantized, numBits);
}

void BitWriter::writeVarInt(uint64_t value) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        writeU8(byte);
    } while (value != 0);
}

void BitWriter::writeBytes(std::span<const uint8_t> data) {
    for (uint8_t byte : data)
        writeU8(byte);
}

void BitWriter::writeString(std::string_view str) {
    writeVarInt(str.size());
    for (char c : str)
        writeU8(static_cast<uint8_t>(c));
}

void BitWriter::alignToByte() {
    if (m_bitCursor % 8 != 0) {
        unsigned pad = 8 - static_cast<unsigned>(m_bitCursor % 8);
        writeBits(0, pad);
    }
}

void BitWriter::clear() {
    m_buffer.clear();
    m_bitCursor = 0;
}

// ============================================================================
// BitReader
// ============================================================================

BitReader::BitReader(std::span<const uint8_t> data) : m_data(data) {}

uint64_t BitReader::readBits(unsigned numBits) {
    if (m_error || numBits == 0 || numBits > 64) return 0;
    if (numBits > bitsRemaining()) {
        m_error = true;
        return 0;
    }

    // Byte-aligned fast path
    if ((m_bitCursor % 8) == 0 && (numBits % 8) == 0) {
        size_t bytePos = m_bitCursor / 8;
        unsigned byteCount = numBits / 8;
        uint64_t value = 0;
        for (unsigned i = 0; i < byteCount; ++i)
            value |= static_cast<uint64_t>(m_data[bytePos + i]) << (i * 8);
        m_bitCursor += numBits;
        return value;
    }

    uint64_t value = 0;
    for (unsigned i = 0; i < numBits; ++i) {
        if ((m_data[m_bitCursor / 8] >> (m_bitCursor % 8)) & 1)
            value |= uint64_t{1} << i;
        ++m_bitCursor;
    }
    return value;
}

float BitReader::readFloat() {
    return std::bit_cast<float>(static_cast<uint32_t>(readBits(32)));
}

double BitReader::readDouble() {
    return std::bit_cast<double>(readBits(64));
}

float BitReader::readQuantizedFloat(float min, float max, unsigned numBits) {
    if (numBits == 0 || numBits > 32 || max <= min) { m_error = true; return 0.0f; }
    uint32_t maxInt = (numBits == 32) ? 0xFFFFFFFFu : ((1u << numBits) - 1u);
    auto quantized = static_cast<uint32_t>(readBits(numBits));
    if (m_error) return 0.0f;
    float t = static_cast<float>(quantized) / static_cast<float>(maxInt);
    return min + t * (max - min);
}

uint64_t BitReader::readVarInt() {
    uint64_t value = 0;
    unsigned shift = 0;
    for (unsigned i = 0; i < 10; ++i) {
        uint8_t byte = readU8();
        if (m_error) return 0;
        value |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) return value;
        shift += 7;
    }
    m_error = true;  // unterminated varint
    return 0;
}

bool BitReader::readBytes(std::span<uint8_t> out) {
    for (uint8_t& byte : out) {
        byte = readU8();
        if (m_error) return false;
    }
    return true;
}

std::string BitReader::readString() {
    uint64_t len = readVarInt();
    if (m_error || len > bitsRemaining() / 8) {
        m_error = true;
        return {};
    }
    std::string str;
    str.reserve(len);
    for (uint64_t i = 0; i < len; ++i)
        str.push_back(static_cast<char>(readU8()));
    if (m_error) return {};
    return str;
}

void BitReader::alignToByte() {
    if (m_bitCursor % 8 != 0)
        m_bitCursor += 8 - (m_bitCursor % 8);
}

size_t BitReader::bitsRemaining() const {
    size_t total = m_data.size() * 8;
    return m_bitCursor <= total ? total - m_bitCursor : 0;
}

} // namespace OmniNet::Game
