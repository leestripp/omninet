// OmniNet - Per-peer game connection state: channels, seq/ack, retransmission,
// message aggregation, RTT estimation, fragmentation
#ifndef OMNINET_GAME_CONNECTION_H
#define OMNINET_GAME_CONNECTION_H
#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

#include "game/GameProtocol.h"
#include "net/UdpSocket.h"

namespace OmniNet::Game {

using ConnectionId = uint16_t;
using SteadyClock = std::chrono::steady_clock;
using SteadyTime = SteadyClock::time_point;

// ============================================================================
// ConnectionStats
// ============================================================================

struct ConnectionStats {
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t messagesSent = 0;
    uint64_t messagesDelivered = 0;
    uint64_t retransmits = 0;
    uint64_t duplicatesDropped = 0;
    uint64_t staleSequencedDropped = 0;
    uint64_t fragmentsReceived = 0;
    uint64_t reassembliesCompleted = 0;
    uint64_t reassembliesDropped = 0;   // expired or oversized
};

// ============================================================================
// GameConnection
// ============================================================================

// Per-peer state machine shared by GameServer (one per client) and GameClient
// (one for the server). Implements the QuakeWorld/Fiedler reliability model:
// packet-level seq + piggyback ack/ackBits, reliable payloads retransmitted
// with a new packet seq until acked, per-channel msgSeq restores ordering.
//
// Phase 2 additions:
//  - Aggregation: queueMessage() stages records; buildPackets() packs as many
//    as fit into each packet (amortizes the 14-byte header).
//  - Adaptive RTO: Jacobson/Karels RTT estimator with Karn's rule.
//  - Fragmentation: reliable messages larger than one packet are split and
//    reassembled transparently.
//
// Not thread-safe: drive from a single pump() thread.
class GameConnection {
public:
    using DeliverCallback =
        std::function<void(uint8_t channel, std::span<const uint8_t> payload)>;

    GameConnection(UdpEndpoint remote, ConnectionId id,
                   uint8_t channelCount = DEFAULT_CHANNEL_COUNT);

    // --- Configuration ---

    void setChannelMode(uint8_t channel, ChannelMode mode);
    // Minimum retransmission timeout (floor for the adaptive RTO)
    void setResendInterval(std::chrono::milliseconds interval) { m_minRto = interval; }
    void setKeepaliveInterval(std::chrono::milliseconds interval) { m_keepaliveInterval = interval; }
    // How long an incomplete fragmented message is kept before being dropped
    void setReassemblyTimeout(std::chrono::milliseconds timeout) { m_reassemblyTimeout = timeout; }

    const UdpEndpoint& remote() const { return m_remote; }
    ConnectionId connectionId() const { return m_connectionId; }
    void setConnectionId(ConnectionId id) { m_connectionId = id; }

    const ConnectionStats& stats() const { return m_stats; }

    // --- RTT estimation (Jacobson/Karels) ---

    float smoothedRttMs() const { return m_srttMs; }
    float rttVarMs() const { return m_rttvarMs; }
    std::chrono::milliseconds currentRto() const;
    uint64_t rttSampleCount() const { return m_rttSamples; }

    // --- Outgoing ---

    // Queue a message for the next packet. Reliable oversized messages are
    // fragmented automatically. Returns false for an invalid channel, an
    // oversized message on a non-reliable channel, or > GAME_MAX_MESSAGE.
    bool queueMessage(uint8_t channel, std::span<const uint8_t> payload);

    // Pack queued messages into packets (<= GAME_MTU each) and append them
    // to outPackets. Reliable records are tracked for retransmission.
    void buildPackets(std::vector<std::vector<uint8_t>>& outPackets);

    // Re-queue reliable records whose RTO has elapsed, then rebuild packets.
    void collectRetransmits(SteadyTime now,
                            std::vector<std::vector<uint8_t>>& outPackets);

    // True when it is time to send a keepalive ping
    bool needsKeepalive(SteadyTime now) const;

    // True when messages are staged but not yet packed
    bool hasQueuedMessages() const { return !m_outQueue.empty(); }

    // --- Incoming ---

    // Update ack state from a received packet header (call for every packet)
    void onPacketReceived(const PacketHeader& header);

    // Process one message record; routes through reassembly/ordering/dedup
    // and invokes deliver for messages ready for the application.
    void processMessage(const MessageRecord& record,
                        const DeliverCallback& deliver);

    // Convenience: decode a full datagram (header + records) and process it.
    // Returns false on malformed data.
    bool processPacket(std::span<const uint8_t> datagram,
                       const DeliverCallback& deliver,
                       PacketHeader* outHeader = nullptr);

    // --- Timing ---

    SteadyTime lastReceiveTime() const { return m_lastReceiveTime; }
    void touch() { m_lastReceiveTime = SteadyClock::now(); }

private:
    struct ChannelState {
        ChannelMode mode = ChannelMode::Reliable;
        // outgoing
        uint32_t nextMsgSeq = 1;  // 1-based; 0 means "no seq" on the wire
        // incoming reliable-ordered
        uint32_t expectedMsgSeq = 1;
        std::map<uint32_t, std::vector<uint8_t>> holdback;
        // incoming unreliable-sequenced
        uint32_t lastSequencedMsgSeq = 0;
        bool hasSequenced = false;
    };

    // One staged outgoing record (post-fragmentation)
    struct OutgoingRecord {
        uint8_t channel = 0;
        uint8_t flags = 0;
        uint32_t msgSeq = 0;
        uint16_t fragmentIndex = 0;
        uint16_t fragmentCount = 0;
        std::vector<uint8_t> payload;

        size_t recordSize() const {
            size_t hdr = (flags & PacketFlags::Fragment)
                             ? FRAGMENT_HEADER_SIZE : MESSAGE_HEADER_SIZE;
            return hdr + payload.size();
        }
        bool reliable() const { return (flags & PacketFlags::Reliable) != 0; }
    };

    // In-flight packet awaiting ack
    struct PendingPacket {
        std::vector<OutgoingRecord> records;  // reliable records only
        SteadyTime sentTime;
        uint32_t resendCount = 0;
    };

    // Partial reassembly state for one fragmented message
    struct Reassembly {
        std::vector<uint8_t> buffer;
        std::vector<bool> received;
        uint16_t receivedCount = 0;
        uint16_t lastFragmentSize = 0;  // payload size of fragment (count-1)
        SteadyTime lastUpdate;
    };

    void queueFragmented(ChannelState& ch, uint8_t channel,
                         std::span<const uint8_t> payload);
    void deliverOrdered(ChannelState& ch, uint8_t channel, uint32_t msgSeq,
                        std::span<const uint8_t> payload,
                        const DeliverCallback& deliver);
    void processAcks(uint32_t ack, uint32_t ackBits);
    void sampleRtt(SteadyTime sentTime);
    void sweepReassemblies(SteadyTime now);

    UdpEndpoint m_remote;
    ConnectionId m_connectionId = 0;
    std::vector<ChannelState> m_channels;

    // packet-level seq/ack state
    uint32_t m_outgoingPacketSeq = 0;
    uint32_t m_remoteSeq = 0;
    uint32_t m_ackBits = 0;
    bool m_receivedAny = false;

    std::deque<OutgoingRecord> m_outQueue;
    std::unordered_map<uint32_t, PendingPacket> m_pending;  // keyed by packetSeq

    // RTT estimation (ms, float for cheap EWMA)
    float m_srttMs = -1.0f;    // <0 = no sample yet
    float m_rttvarMs = 0.0f;
    uint64_t m_rttSamples = 0;
    std::chrono::milliseconds m_minRto{100};
    std::chrono::milliseconds m_keepaliveInterval{1000};
    static constexpr float MAX_RTO_MS = 3000.0f;

    // Fragment reassembly: key = (channel << 32) | msgSeq
    std::unordered_map<uint64_t, Reassembly> m_reassembly;
    std::chrono::milliseconds m_reassemblyTimeout{5000};

    SteadyTime m_lastReceiveTime = SteadyClock::now();
    SteadyTime m_lastSendTime = SteadyTime::min();

    ConnectionStats m_stats;
};

} // namespace OmniNet::Game

#endif // OMNINET_GAME_CONNECTION_H
