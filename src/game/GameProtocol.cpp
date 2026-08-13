// OmniNet - Game networking wire protocol
#include "game/GameProtocol.h"

#include <cstring>

namespace OmniNet::Game {

namespace {

inline void writeU16LE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void writeU32LE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

// ============================================================================
// Packet header
// ============================================================================

void encodePacketHeader(uint8_t* out, const PacketHeader& header) {
    writeU16LE(out + 0,  header.connectionId);
    writeU32LE(out + 2,  header.packetSeq);
    writeU32LE(out + 6,  header.ack);
    writeU32LE(out + 10, header.ackBits);
}

bool decodePacketHeader(std::span<const uint8_t> datagram,
                        PacketHeader& header,
                        std::span<const uint8_t>& records) {
    if (datagram.size() < PACKET_HEADER_SIZE)
        return false;

    const uint8_t* p = datagram.data();
    header.connectionId = readU16LE(p + 0);
    header.packetSeq    = readU32LE(p + 2);
    header.ack          = readU32LE(p + 6);
    header.ackBits      = readU32LE(p + 10);
    records = datagram.subspan(PACKET_HEADER_SIZE);
    return true;
}

// ============================================================================
// Message records
// ============================================================================

void encodeRecord(std::vector<uint8_t>& packet,
                  uint8_t channel, uint8_t flags, uint32_t msgSeq,
                  uint16_t fragmentIndex, uint16_t fragmentCount,
                  std::span<const uint8_t> payload) {
    const bool fragmented = (flags & PacketFlags::Fragment) != 0;
    size_t headerSize = fragmented ? FRAGMENT_HEADER_SIZE : MESSAGE_HEADER_SIZE;
    size_t base = packet.size();
    packet.resize(base + headerSize + payload.size());
    uint8_t* p = packet.data() + base;

    p[0] = channel;
    p[1] = flags;
    writeU32LE(p + 2, msgSeq);
    size_t offset = 6;
    if (fragmented) {
        writeU16LE(p + 6, fragmentIndex);
        writeU16LE(p + 8, fragmentCount);
        offset = 10;
    }
    writeU16LE(p + offset, static_cast<uint16_t>(payload.size()));
    if (!payload.empty())
        std::memcpy(p + offset + 2, payload.data(), payload.size());
}

bool decodeRecord(std::span<const uint8_t>& records, MessageRecord& out) {
    if (records.size() < MESSAGE_HEADER_SIZE)
        return false;

    const uint8_t* p = records.data();
    out.channel = p[0];
    out.flags = p[1];
    out.msgSeq = readU32LE(p + 2);

    size_t offset = 6;
    if (out.flags & PacketFlags::Fragment) {
        if (records.size() < FRAGMENT_HEADER_SIZE)
            return false;
        out.fragmentIndex = readU16LE(p + 6);
        out.fragmentCount = readU16LE(p + 8);
        offset = 10;
    } else {
        out.fragmentIndex = 0;
        out.fragmentCount = 0;
    }

    uint16_t payloadLen = readU16LE(p + offset);
    if (records.size() < offset + 2 + payloadLen)
        return false;

    out.payload = records.subspan(offset + 2, payloadLen);
    records = records.subspan(offset + 2 + payloadLen);
    return true;
}

} // namespace OmniNet::Game
