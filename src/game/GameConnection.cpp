// OmniNet - Per-peer game connection state
#include "game/GameConnection.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace OmniNet::Game {

GameConnection::GameConnection(UdpEndpoint remote, ConnectionId id,
                               uint8_t channelCount)
    : m_remote(std::move(remote)), m_connectionId(id),
      m_channels(channelCount) {}

void GameConnection::setChannelMode(uint8_t channel, ChannelMode mode) {
    if (channel < m_channels.size())
        m_channels[channel].mode = mode;
}

// ============================================================================
// RTT estimation (Jacobson/Karels, RFC 6298 style)
// ============================================================================

std::chrono::milliseconds GameConnection::currentRto() const {
    float rto = (m_srttMs < 0.0f)
                    ? static_cast<float>(m_minRto.count())
                    : m_srttMs + 4.0f * m_rttvarMs;
    float floorMs = static_cast<float>(m_minRto.count());
    rto = std::clamp(rto, floorMs, MAX_RTO_MS);
    return std::chrono::milliseconds(static_cast<int64_t>(rto));
}

void GameConnection::sampleRtt(SteadyTime sentTime) {
    float sample = std::chrono::duration<float, std::milli>(
        SteadyClock::now() - sentTime).count();
    if (sample < 0.0f) return;

    ++m_rttSamples;
    if (m_srttMs < 0.0f) {
        m_srttMs = sample;
        m_rttvarMs = sample / 2.0f;
    } else {
        float err = std::fabs(sample - m_srttMs);
        m_rttvarMs = 0.75f * m_rttvarMs + 0.25f * err;
        m_srttMs = 0.875f * m_srttMs + 0.125f * sample;
    }
}

// ============================================================================
// Outgoing
// ============================================================================

bool GameConnection::queueMessage(uint8_t channel,
                                  std::span<const uint8_t> payload) {
    if (channel >= m_channels.size()) return false;
    if (payload.size() > GAME_MAX_MESSAGE) return false;

    ChannelState& ch = m_channels[channel];

    if (ch.mode == ChannelMode::Reliable && payload.size() > GAME_MAX_PAYLOAD) {
        queueFragmented(ch, channel, payload);
        return true;
    }
    if (payload.size() > GAME_MAX_PAYLOAD) return false;  // non-reliable: no fragmentation

    OutgoingRecord rec;
    rec.channel = channel;
    switch (ch.mode) {
        case ChannelMode::Reliable:
            rec.flags = PacketFlags::Reliable | PacketFlags::Sequenced;
            rec.msgSeq = ch.nextMsgSeq++;
            break;
        case ChannelMode::UnreliableSequenced:
            rec.flags = PacketFlags::Sequenced;
            rec.msgSeq = ch.nextMsgSeq++;
            break;
        case ChannelMode::Unreliable:
            rec.flags = 0;
            break;
    }
    rec.payload.assign(payload.begin(), payload.end());
    m_outQueue.push_back(std::move(rec));
    ++m_stats.messagesSent;
    return true;
}

void GameConnection::queueFragmented(ChannelState& ch, uint8_t channel,
                                     std::span<const uint8_t> payload) {
    const size_t fragPayload = GAME_MTU - PACKET_HEADER_SIZE - FRAGMENT_HEADER_SIZE;
    uint16_t count = static_cast<uint16_t>((payload.size() + fragPayload - 1) / fragPayload);
    uint32_t msgSeq = ch.nextMsgSeq++;  // one msgSeq shared by all fragments

    for (uint16_t i = 0; i < count; ++i) {
        OutgoingRecord rec;
        rec.channel = channel;
        rec.flags = PacketFlags::Reliable | PacketFlags::Sequenced | PacketFlags::Fragment;
        rec.msgSeq = msgSeq;
        rec.fragmentIndex = i;
        rec.fragmentCount = count;
        size_t offset = static_cast<size_t>(i) * fragPayload;
        size_t len = std::min(fragPayload, payload.size() - offset);
        rec.payload.assign(payload.begin() + offset, payload.begin() + offset + len);
        m_outQueue.push_back(std::move(rec));
    }
    ++m_stats.messagesSent;
}

void GameConnection::buildPackets(std::vector<std::vector<uint8_t>>& outPackets) {
    while (!m_outQueue.empty()) {
        std::vector<uint8_t> packet(PACKET_HEADER_SIZE);
        std::vector<OutgoingRecord> reliableRecords;

        // Greedily pack records until the packet is full
        while (!m_outQueue.empty()) {
            const OutgoingRecord& front = m_outQueue.front();
            if (packet.size() + front.recordSize() > GAME_MTU && packet.size() > PACKET_HEADER_SIZE)
                break;  // packet full (but never emit an empty packet)
            if (packet.size() + front.recordSize() > GAME_MTU) {
                // Single record larger than MTU should never happen after
                // queueMessage's fragmentation; drop defensively.
                m_outQueue.pop_front();
                continue;
            }
            encodeRecord(packet, front.channel, front.flags, front.msgSeq,
                         front.fragmentIndex, front.fragmentCount, front.payload);
            if (front.reliable())
                reliableRecords.push_back(std::move(m_outQueue.front()));
            m_outQueue.pop_front();
        }

        if (packet.size() == PACKET_HEADER_SIZE) break;  // nothing fit

        PacketHeader h;
        h.connectionId = m_connectionId;
        h.packetSeq = m_outgoingPacketSeq++;
        h.ack = m_receivedAny ? m_remoteSeq : 0;
        h.ackBits = m_receivedAny ? m_ackBits : 0;
        encodePacketHeader(packet.data(), h);

        if (!reliableRecords.empty()) {
            PendingPacket pending;
            pending.records = std::move(reliableRecords);
            pending.sentTime = SteadyClock::now();
            m_pending.emplace(h.packetSeq, std::move(pending));
        }

        m_lastSendTime = SteadyClock::now();
        ++m_stats.packetsSent;
        outPackets.push_back(std::move(packet));
    }
}

void GameConnection::sweepReassemblies(SteadyTime now) {
    for (auto it = m_reassembly.begin(); it != m_reassembly.end();) {
        if (now - it->second.lastUpdate >= m_reassemblyTimeout) {
            // Advance the ordered sequence past the dropped message so the
            // channel is not blocked forever by the gap. Late retransmitted
            // fragments for it are then dropped as duplicates. Messages
            // already in the holdback drain on the next in-order delivery.
            uint8_t channel = static_cast<uint8_t>(it->first >> 32);
            uint32_t msgSeq = static_cast<uint32_t>(it->first & 0xFFFFFFFFu);
            if (channel < m_channels.size()) {
                ChannelState& ch = m_channels[channel];
                if (msgSeq == ch.expectedMsgSeq)
                    ++ch.expectedMsgSeq;
            }
            it = m_reassembly.erase(it);
            ++m_stats.reassembliesDropped;
        } else {
            ++it;
        }
    }
}

void GameConnection::collectRetransmits(SteadyTime now,
                                        std::vector<std::vector<uint8_t>>& outPackets) {
    auto rto = currentRto();
    sweepReassemblies(now);

    // Re-queue expired reliable records at the FRONT so they go out first
    std::deque<OutgoingRecord> resend;
    std::vector<uint32_t> expiredSeqs;
    for (auto& [seq, pending] : m_pending) {
        if (now - pending.sentTime >= rto) {
            for (auto& rec : pending.records)
                resend.push_back(std::move(rec));
            ++m_stats.retransmits;
            expiredSeqs.push_back(seq);
        }
    }
    for (uint32_t seq : expiredSeqs)
        m_pending.erase(seq);

    if (resend.empty()) return;

    // Rebuild packets for the resent records; track them as new pending
    // entries with an incremented resend count (Karn's rule: no RTT sample
    // will be taken for these).
    while (!resend.empty()) {
        std::vector<uint8_t> packet(PACKET_HEADER_SIZE);
        std::vector<OutgoingRecord> packed;

        while (!resend.empty()) {
            const OutgoingRecord& front = resend.front();
            if (packet.size() + front.recordSize() > GAME_MTU && packet.size() > PACKET_HEADER_SIZE)
                break;
            encodeRecord(packet, front.channel, front.flags, front.msgSeq,
                         front.fragmentIndex, front.fragmentCount, front.payload);
            packed.push_back(std::move(resend.front()));
            resend.pop_front();
        }
        if (packet.size() == PACKET_HEADER_SIZE) break;

        PacketHeader h;
        h.connectionId = m_connectionId;
        h.packetSeq = m_outgoingPacketSeq++;
        h.ack = m_receivedAny ? m_remoteSeq : 0;
        h.ackBits = m_receivedAny ? m_ackBits : 0;
        encodePacketHeader(packet.data(), h);

        PendingPacket pending;
        pending.records = std::move(packed);
        pending.sentTime = now;
        pending.resendCount = 1;
        m_pending.emplace(h.packetSeq, std::move(pending));

        m_lastSendTime = now;
        ++m_stats.packetsSent;
        outPackets.push_back(std::move(packet));
    }
}

bool GameConnection::needsKeepalive(SteadyTime now) const {
    return now - m_lastSendTime >= m_keepaliveInterval;
}

// ============================================================================
// Incoming
// ============================================================================

void GameConnection::processAcks(uint32_t ack, uint32_t ackBits) {
    auto dropPending = [this](uint32_t packetSeq) {
        auto it = m_pending.find(packetSeq);
        if (it == m_pending.end()) return;
        // Karn's rule: only sample RTT for packets never retransmitted
        if (it->second.resendCount == 0)
            sampleRtt(it->second.sentTime);
        m_pending.erase(it);
    };

    if (!m_receivedAny) return;
    dropPending(ack);
    for (unsigned i = 0; i < 32; ++i) {
        if (ackBits & (1u << i))
            dropPending(ack - 1 - i);
    }
}

void GameConnection::onPacketReceived(const PacketHeader& header) {
    ++m_stats.packetsReceived;
    m_lastReceiveTime = SteadyClock::now();

    // --- Update our ack window for the peer's packet seq ---
    if (!m_receivedAny) {
        m_remoteSeq = header.packetSeq;
        m_ackBits = 0;
        m_receivedAny = true;
    } else if (seqGreaterThan(header.packetSeq, m_remoteSeq)) {
        uint32_t shift = header.packetSeq - m_remoteSeq;
        if (shift > 32) {
            m_ackBits = 0;
        } else {
            m_ackBits = (shift == 32) ? 0u : (m_ackBits << shift);
        }
        if (shift <= 32)
            m_ackBits |= (1u << (shift - 1));
        m_remoteSeq = header.packetSeq;
    } else if (header.packetSeq != m_remoteSeq) {
        uint32_t back = m_remoteSeq - header.packetSeq;
        if (back >= 1 && back <= 32)
            m_ackBits |= (1u << (back - 1));
    }
    // duplicate of remoteSeq: nothing to update

    processAcks(header.ack, header.ackBits);
}

void GameConnection::deliverOrdered(ChannelState& ch, uint8_t channel,
                                    uint32_t msgSeq,
                                    std::span<const uint8_t> payload,
                                    const DeliverCallback& deliver) {
    if (msgSeq == ch.expectedMsgSeq) {
        deliver(channel, payload);
        ++m_stats.messagesDelivered;
        ++ch.expectedMsgSeq;
        // Drain holdback of subsequent in-order messages
        while (true) {
            auto it = ch.holdback.find(ch.expectedMsgSeq);
            if (it == ch.holdback.end()) break;
            deliver(channel, it->second);
            ++m_stats.messagesDelivered;
            ++ch.expectedMsgSeq;
            ch.holdback.erase(it);
        }
    } else if (seqGreaterThan(msgSeq, ch.expectedMsgSeq)) {
        if (ch.holdback.find(msgSeq) == ch.holdback.end())
            ch.holdback.emplace(msgSeq,
                                std::vector<uint8_t>(payload.begin(), payload.end()));
        else
            ++m_stats.duplicatesDropped;
    } else {
        ++m_stats.duplicatesDropped;  // retransmit of already-delivered
    }
}

void GameConnection::processMessage(const MessageRecord& record,
                                    const DeliverCallback& deliver) {
    if (record.channel >= m_channels.size()) return;
    ChannelState& ch = m_channels[record.channel];

    const bool reliable = (record.flags & PacketFlags::Reliable) != 0;
    const bool sequenced = (record.flags & PacketFlags::Sequenced) != 0;
    const bool fragment = (record.flags & PacketFlags::Fragment) != 0;

    if (fragment) {
        // Fragments only valid on reliable records
        if (!reliable || record.fragmentCount == 0 ||
            record.fragmentIndex >= record.fragmentCount)
            return;

        // Already delivered or superseded: drop silently as duplicate
        if (!seqGreaterOrEqual(record.msgSeq, ch.expectedMsgSeq)) {
            ++m_stats.duplicatesDropped;
            return;
        }

        size_t fragPayload = GAME_MTU - PACKET_HEADER_SIZE - FRAGMENT_HEADER_SIZE;
        // Upper-bound guard: exact size is known once the tail fragment arrives
        if (fragPayload * record.fragmentCount > GAME_MAX_MESSAGE + fragPayload) {
            ++m_stats.reassembliesDropped;
            return;
        }

        uint64_t key = (static_cast<uint64_t>(record.channel) << 32) | record.msgSeq;
        auto& re = m_reassembly[key];
        if (re.received.empty()) {
            re.buffer.resize(fragPayload * record.fragmentCount);
            re.received.assign(record.fragmentCount, false);
        }
        re.lastUpdate = SteadyClock::now();

        if (record.fragmentIndex == record.fragmentCount - 1)
            re.lastFragmentSize = static_cast<uint16_t>(record.payload.size());

        if (!re.received[record.fragmentIndex]) {
            size_t offset = static_cast<size_t>(record.fragmentIndex) * fragPayload;
            std::copy(record.payload.begin(), record.payload.end(),
                      re.buffer.begin() + offset);
            re.received[record.fragmentIndex] = true;
            ++re.receivedCount;
            ++m_stats.fragmentsReceived;
        } else {
            ++m_stats.duplicatesDropped;
        }

        if (re.receivedCount == record.fragmentCount) {
            // Trim to the true message size using the tail fragment's size
            size_t realSize = fragPayload * (record.fragmentCount - 1) + re.lastFragmentSize;
            re.buffer.resize(realSize);
            deliverOrdered(ch, record.channel, record.msgSeq, re.buffer, deliver);
            m_reassembly.erase(key);
            ++m_stats.reassembliesCompleted;
        }
        return;
    }

    if (reliable) {
        deliverOrdered(ch, record.channel, record.msgSeq, record.payload, deliver);
    } else if (sequenced) {
        if (!ch.hasSequenced || seqGreaterThan(record.msgSeq, ch.lastSequencedMsgSeq)) {
            ch.lastSequencedMsgSeq = record.msgSeq;
            ch.hasSequenced = true;
            deliver(record.channel, record.payload);
            ++m_stats.messagesDelivered;
        } else {
            ++m_stats.staleSequencedDropped;
        }
    } else {
        deliver(record.channel, record.payload);
        ++m_stats.messagesDelivered;
    }
}

bool GameConnection::processPacket(std::span<const uint8_t> datagram,
                                   const DeliverCallback& deliver,
                                   PacketHeader* outHeader) {
    PacketHeader header;
    std::span<const uint8_t> records;
    if (!decodePacketHeader(datagram, header, records))
        return false;

    onPacketReceived(header);
    if (outHeader) *outHeader = header;

    while (!records.empty()) {
        MessageRecord rec;
        if (!decodeRecord(records, rec))
            return false;
        processMessage(rec, deliver);
    }
    return true;
}

} // namespace OmniNet::Game
