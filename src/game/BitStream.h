// OmniNet - Bit-level serialization for high-speed game networking
#ifndef OMNINET_GAME_BITSTREAM_H
#define OMNINET_GAME_BITSTREAM_H
#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OmniNet::Game {

// ============================================================================
// BitWriter
// ============================================================================

// High-speed bit-level writer. Values are packed little-endian bit order
// (first bit written is the least significant). Zero-copy access to the
// resulting byte buffer via bytes().
class BitWriter {
public:
    BitWriter();
    explicit BitWriter(size_t reserveBytes);

    // --- Bit-level primitives ---

    // Write the lowest numBits of value (1..64)
    void writeBits(uint64_t value, unsigned numBits);
    void writeBool(bool value) { writeBits(value ? 1 : 0, 1); }

    // --- Aligned-friendly helpers (fast path when byte-aligned) ---

    void writeU8(uint8_t v)   { writeBits(v, 8); }
    void writeU16(uint16_t v) { writeBits(v, 16); }
    void writeU32(uint32_t v) { writeBits(v, 32); }
    void writeU64(uint64_t v) { writeBits(v, 64); }
    void writeFloat(float f);
    void writeDouble(double d);

    // Quantized float in [min, max] with numBits precision.
    // 12 bits over [-100, 100] gives ~0.05 resolution.
    void writeQuantizedFloat(float value, float min, float max, unsigned numBits);

    // Unsigned LEB128 varint (1-10 bytes)
    void writeVarInt(uint64_t value);

    // Raw bytes (does not byte-align; use alignToByte first if the reader
    // expects alignment)
    void writeBytes(std::span<const uint8_t> data);

    // String with varint length prefix
    void writeString(std::string_view str);

    // Pad with zero bits to the next byte boundary
    void alignToByte();

    // --- State ---

    size_t bitsWritten() const { return m_bitCursor; }
    size_t bytesWritten() const { return (m_bitCursor + 7) / 8; }

    // Final byte buffer (last byte zero-padded). Valid until next write.
    std::span<const uint8_t> bytes() const {
        return {m_buffer.data(), bytesWritten()};
    }

    void clear();

private:
    std::vector<uint8_t> m_buffer;
    size_t m_bitCursor = 0;
};

// ============================================================================
// BitReader
// ============================================================================

// Bit-level reader over a borrowed byte span. Sets an error flag on overread;
// all subsequent reads return 0. Check hasError() once at the end.
class BitReader {
public:
    explicit BitReader(std::span<const uint8_t> data);

    uint64_t readBits(unsigned numBits);
    bool     readBool() { return readBits(1) != 0; }

    uint8_t  readU8()   { return static_cast<uint8_t>(readBits(8)); }
    uint16_t readU16()  { return static_cast<uint16_t>(readBits(16)); }
    uint32_t readU32()  { return static_cast<uint32_t>(readBits(32)); }
    uint64_t readU64()  { return readBits(64); }
    float    readFloat();
    double   readDouble();
    float    readQuantizedFloat(float min, float max, unsigned numBits);

    uint64_t readVarInt();

    // Read n raw bytes into out. Returns false (and sets error) if too few
    // bits remain.
    bool readBytes(std::span<uint8_t> out);

    std::string readString();

    void alignToByte();

    // --- State ---

    size_t bitsRead() const { return m_bitCursor; }
    size_t bitsRemaining() const;
    bool   hasError() const { return m_error; }

private:
    std::span<const uint8_t> m_data;
    size_t m_bitCursor = 0;
    bool   m_error = false;
};

} // namespace OmniNet::Game

#endif // OMNINET_GAME_BITSTREAM_H
