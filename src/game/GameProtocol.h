// OmniNet - Game networking wire protocol (header-only constants + header codec)
#ifndef OMNINET_GAME_PROTOCOL_H
#define OMNINET_GAME_PROTOCOL_H
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace OmniNet::Game {

// ============================================================================
// Protocol constants
// ============================================================================

inline constexpr uint16_t GAME_PROTOCOL_VERSION = 3;

// Maximum UDP packet we will build: conservative MTU minus IP/UDP overhead
inline constexpr size_t GAME_MTU = 1200;

// Maximum reassembled message size (fragmented reliable messages)
inline constexpr size_t GAME_MAX_MESSAGE = 256 * 1024;

// Channel 0 is reserved for system messages (handshake, ping, disconnect)
inline constexpr uint8_t SYSTEM_CHANNEL = 0;

// Default user channels
inline constexpr uint8_t DEFAULT_CHANNEL_COUNT = 8;

// ============================================================================
// Channel modes
// ============================================================================

enum class ChannelMode : uint8_t {
    Reliable,             // guaranteed + ordered   (chat, RPC, inventory)
    UnreliableSequenced,  // not guaranteed, latest-wins (position snapshots)
    Unreliable,           // fire and forget (effects, voice)
};

// ============================================================================
// Wire format (little-endian)
// ============================================================================
//
// Packet header (14 bytes):
//   u16 connectionId   (0 = not yet assigned / handshake)
//   u32 packetSeq
//   u32 ack            highest packet seq received from peer
//   u32 ackBits        bit i set => packet (ack - 1 - i) was received
//
// Then one or more message records until end of datagram:
//   u8  channel
//   u8  flags          bit0 reliable | bit1 sequenced | bit2 fragment
//   u32 msgSeq         per-channel message sequence (0 when N/A)
//   [u16 fragmentIndex, u16 fragmentCount]   only when bit2 set
//   u16 payloadLen
//   ... payload
//
inline constexpr size_t PACKET_HEADER_SIZE  = 14;
inline constexpr size_t MESSAGE_HEADER_SIZE = 8;    // without fragment fields
inline constexpr size_t FRAGMENT_HEADER_SIZE = 12;  // with fragment fields

// Largest single record payload that fits in one packet
inline constexpr size_t GAME_MAX_PAYLOAD =
    GAME_MTU - PACKET_HEADER_SIZE - MESSAGE_HEADER_SIZE;

namespace PacketFlags {
    inline constexpr uint8_t Reliable  = 1 << 0;
    inline constexpr uint8_t Sequenced = 1 << 1;
    inline constexpr uint8_t Fragment  = 1 << 2;
}

struct PacketHeader {
    uint16_t connectionId = 0;
    uint32_t packetSeq = 0;
    uint32_t ack = 0;
    uint32_t ackBits = 0;
};

// One parsed message record inside a packet
struct MessageRecord {
    uint8_t  channel = 0;
    uint8_t  flags = 0;
    uint32_t msgSeq = 0;
    uint16_t fragmentIndex = 0;  // valid when flags & Fragment
    uint16_t fragmentCount = 0;
    std::span<const uint8_t> payload;  // points into the source datagram
};

// Encode the 14-byte packet header at the start of out (out must be >= 14)
void encodePacketHeader(uint8_t* out, const PacketHeader& header);

// Decode the packet header; returns false if datagram < 14 bytes.
// records is set to the remaining bytes (message record area).
bool decodePacketHeader(std::span<const uint8_t> datagram,
                        PacketHeader& header,
                        std::span<const uint8_t>& records);

// Append one message record to a packet buffer
void encodeRecord(std::vector<uint8_t>& packet,
                  uint8_t channel, uint8_t flags, uint32_t msgSeq,
                  uint16_t fragmentIndex, uint16_t fragmentCount,
                  std::span<const uint8_t> payload);

// Parse the next record from the record area. On success returns true and
// advances records past the parsed record. Returns false on malformed data.
bool decodeRecord(std::span<const uint8_t>& records, MessageRecord& out);

// ============================================================================
// System messages (channel 0 payload, first byte = type)
// ============================================================================

namespace SystemMsg {
    inline constexpr uint8_t Connect    = 1;  // + u16 protocol version
    inline constexpr uint8_t Accept     = 2;  // + u16 assigned connectionId
    inline constexpr uint8_t Deny       = 3;  // server full / bad version
    inline constexpr uint8_t Ping       = 4;  // keepalive (no extra data)
    inline constexpr uint8_t Disconnect = 5;  // graceful close
    inline constexpr uint8_t Challenge  = 6;  // server->client: u32 token (DoS hardening)
}

// Sequence-number comparison that handles wrap-around
inline bool seqGreaterThan(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0;
}

inline bool seqGreaterOrEqual(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) >= 0;
}

} // namespace OmniNet::Game

#endif // OMNINET_GAME_PROTOCOL_H
