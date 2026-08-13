// OmniNet - GameConnection protocol tests (no sockets; packets are exchanged in-memory)
#include "game/GameConnection.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace OmniNet;
using namespace OmniNet::Game;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

namespace {

// Two connections wired back-to-back; packets are exchanged in memory.
struct WirePair {
    GameConnection a{{"127.0.0.1", 1001}, 1};
    GameConnection b{{"127.0.0.1", 1002}, 2};

    std::vector<std::string> deliveredA;
    std::vector<std::string> deliveredB;

    WirePair() {
        a.setChannelMode(1, ChannelMode::Reliable);
        b.setChannelMode(1, ChannelMode::Reliable);
    }

    // Flush a's queue and feed every packet to b. Returns packets sent.
    std::vector<std::vector<uint8_t>> flushAToB() {
        std::vector<std::vector<uint8_t>> packets;
        a.buildPackets(packets);
        for (const auto& p : packets)
            feedFromAToB(p);
        return packets;
    }

    std::vector<std::vector<uint8_t>> flushBToA() {
        std::vector<std::vector<uint8_t>> packets;
        b.buildPackets(packets);
        for (const auto& p : packets)
            feedFromBToA(p);
        return packets;
    }

    void feedFromAToB(const std::vector<uint8_t>& packet) {
        ASSERT_TRUE(b.processPacket(packet, [this](uint8_t, std::span<const uint8_t> d) {
            deliveredB.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
        }));
    }

    void feedFromBToA(const std::vector<uint8_t>& packet) {
        ASSERT_TRUE(a.processPacket(packet, [this](uint8_t, std::span<const uint8_t> d) {
            deliveredA.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
        }));
    }
};

std::vector<uint8_t> toBytes(const std::string& s) {
    return {s.begin(), s.end()};
}

} // namespace

static void test_header_and_record_roundtrip() {
    PacketHeader in;
    in.connectionId = 42;
    in.packetSeq = 1000003;
    in.ack = 999;
    in.ackBits = 0xDEADBEEF;

    std::vector<uint8_t> packet(PACKET_HEADER_SIZE);
    encodePacketHeader(packet.data(), in);

    std::string msg = "payload!";
    encodeRecord(packet, 7, PacketFlags::Reliable | PacketFlags::Sequenced,
                 55, 0, 0, toBytes(msg));
    std::string msg2 = "frag-part";
    encodeRecord(packet, 3, PacketFlags::Reliable | PacketFlags::Fragment,
                 56, 2, 5, toBytes(msg2));

    PacketHeader out;
    std::span<const uint8_t> records;
    ASSERT_TRUE(decodePacketHeader(packet, out, records));
    ASSERT_EQ(out.connectionId, 42);
    ASSERT_EQ(out.packetSeq, 1000003u);
    ASSERT_EQ(out.ack, 999u);
    ASSERT_EQ(out.ackBits, 0xDEADBEEFu);

    MessageRecord r1;
    ASSERT_TRUE(decodeRecord(records, r1));
    ASSERT_EQ(r1.channel, 7);
    ASSERT_EQ(r1.msgSeq, 55u);
    ASSERT_EQ(r1.payload.size(), msg.size());
    ASSERT_TRUE(std::memcmp(r1.payload.data(), msg.data(), msg.size()) == 0);

    MessageRecord r2;
    ASSERT_TRUE(decodeRecord(records, r2));
    ASSERT_EQ(r2.channel, 3);
    ASSERT_TRUE((r2.flags & PacketFlags::Fragment) != 0);
    ASSERT_EQ(r2.fragmentIndex, 2);
    ASSERT_EQ(r2.fragmentCount, 5);
    ASSERT_EQ(r2.payload.size(), msg2.size());

    ASSERT_TRUE(records.empty());

    // malformed: truncated packet rejected
    std::vector<uint8_t> tiny(5, 0);
    ASSERT_TRUE(!decodePacketHeader(tiny, out, records));
}

static void test_reliable_in_order_delivery() {
    WirePair w;
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(w.a.queueMessage(1, toBytes("msg" + std::to_string(i))));
    w.flushAToB();
    ASSERT_EQ(w.deliveredB.size(), 5u);
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(w.deliveredB[i] == "msg" + std::to_string(i));
}

static void test_aggregation_single_packet() {
    WirePair w;
    // 10 small messages should aggregate into ONE packet
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(w.a.queueMessage(1, toBytes("a" + std::to_string(i))));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_EQ(packets.size(), 1u);
    ASSERT_TRUE(packets[0].size() < GAME_MTU);

    for (const auto& p : packets) w.feedFromAToB(p);
    ASSERT_EQ(w.deliveredB.size(), 10u);
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(w.deliveredB[i] == "a" + std::to_string(i));
}

static void test_aggregation_splits_at_mtu() {
    WirePair w;
    // ~100 x 100-byte messages = 10KB total -> multiple packets
    std::string chunk(100, 'x');
    for (int i = 0; i < 100; ++i)
        ASSERT_TRUE(w.a.queueMessage(1, toBytes(chunk)));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_TRUE(packets.size() >= 8);
    for (const auto& p : packets)
        ASSERT_TRUE(p.size() <= GAME_MTU);

    for (const auto& p : packets) w.feedFromAToB(p);
    ASSERT_EQ(w.deliveredB.size(), 100u);
}

static void test_reliable_reorders_out_of_order() {
    WirePair w;
    // Build one packet per message so we can reorder delivery
    std::vector<std::vector<uint8_t>> singles;
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(w.a.queueMessage(1, toBytes(std::to_string(i))));
        std::vector<std::vector<uint8_t>> p;
        w.a.buildPackets(p);
        ASSERT_EQ(p.size(), 1u);
        singles.push_back(p[0]);
    }
    w.feedFromAToB(singles[0]);
    w.feedFromAToB(singles[2]);
    w.feedFromAToB(singles[3]);
    ASSERT_EQ(w.deliveredB.size(), 1u);
    w.feedFromAToB(singles[1]);
    ASSERT_EQ(w.deliveredB.size(), 4u);
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(w.deliveredB[i] == std::to_string(i));
}

static void test_reliable_duplicate_dropped() {
    WirePair w;
    ASSERT_TRUE(w.a.queueMessage(1, toBytes("once")));
    std::vector<std::vector<uint8_t>> p;
    w.a.buildPackets(p);
    w.feedFromAToB(p[0]);
    w.feedFromAToB(p[0]);  // retransmit of same packet
    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.b.stats().duplicatesDropped >= 1);
}

static void test_retransmit_until_acked() {
    WirePair w;
    w.a.setResendInterval(std::chrono::milliseconds(50));

    ASSERT_TRUE(w.a.queueMessage(1, toBytes("important")));
    std::vector<std::vector<uint8_t>> p;
    w.a.buildPackets(p);
    ASSERT_EQ(p.size(), 1u);
    // Packet is "lost" — never delivered to b.

    SteadyTime later = SteadyClock::now() + std::chrono::milliseconds(200);
    std::vector<std::vector<uint8_t>> resends;
    w.a.collectRetransmits(later, resends);
    ASSERT_EQ(resends.size(), 1u);
    ASSERT_TRUE(w.a.stats().retransmits >= 1);

    w.feedFromAToB(resends[0]);
    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.deliveredB[0] == "important");
}

static void test_ack_clears_pending() {
    WirePair w;
    w.a.setResendInterval(std::chrono::milliseconds(50));

    ASSERT_TRUE(w.a.queueMessage(1, toBytes("acked")));
    w.flushAToB();  // b receives it; b's next packet will ack it

    // b sends any packet back; its header carries the ack
    w.b.setChannelMode(2, ChannelMode::Unreliable);
    ASSERT_TRUE(w.b.queueMessage(2, toBytes("reply")));
    w.flushBToA();

    SteadyTime later = SteadyClock::now() + std::chrono::milliseconds(500);
    std::vector<std::vector<uint8_t>> resends;
    w.a.collectRetransmits(later, resends);
    ASSERT_EQ(resends.size(), 0u);
}

static void test_rtt_estimation() {
    WirePair w;
    // Before any ack: no samples, RTO is the floor
    ASSERT_EQ(w.a.rttSampleCount(), 0u);
    ASSERT_TRUE(w.a.currentRto() >= std::chrono::milliseconds(100));

    ASSERT_TRUE(w.a.queueMessage(1, toBytes("rtt probe")));
    w.flushAToB();

    w.b.setChannelMode(2, ChannelMode::Unreliable);
    ASSERT_TRUE(w.b.queueMessage(2, toBytes("ack carrier")));
    w.flushBToA();  // carries ack for a's packet

    ASSERT_TRUE(w.a.rttSampleCount() >= 1);
    ASSERT_TRUE(w.a.smoothedRttMs() >= 0.0f);
    // Loopback RTT is sub-millisecond; RTO must stay sane
    ASSERT_TRUE(w.a.currentRto() < std::chrono::milliseconds(3000));
}

static void test_karn_rule_no_sample_on_resend() {
    WirePair w;
    w.a.setResendInterval(std::chrono::milliseconds(20));

    ASSERT_TRUE(w.a.queueMessage(1, toBytes("lost once")));
    std::vector<std::vector<uint8_t>> p;
    w.a.buildPackets(p);
    // lose original

    SteadyTime later = SteadyClock::now() + std::chrono::milliseconds(100);
    std::vector<std::vector<uint8_t>> resends;
    w.a.collectRetransmits(later, resends);
    ASSERT_EQ(resends.size(), 1u);

    uint64_t samplesBefore = w.a.rttSampleCount();
    // b acks the retransmitted packet; a must NOT sample RTT (Karn's rule)
    w.feedFromAToB(resends[0]);
    ASSERT_TRUE(w.b.queueMessage(1, toBytes("ack")));
    w.flushBToA();
    ASSERT_EQ(w.a.rttSampleCount(), samplesBefore);
}

static void test_unreliable_sequenced_latest_wins() {
    WirePair w;
    w.a.setChannelMode(3, ChannelMode::UnreliableSequenced);
    w.b.setChannelMode(3, ChannelMode::UnreliableSequenced);

    std::vector<std::vector<uint8_t>> singles;
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(w.a.queueMessage(3, toBytes("pos" + std::to_string(i))));
        std::vector<std::vector<uint8_t>> p;
        w.a.buildPackets(p);
        singles.push_back(p[0]);
    }
    w.feedFromAToB(singles[0]);
    w.feedFromAToB(singles[3]);
    w.feedFromAToB(singles[2]);
    w.feedFromAToB(singles[1]);
    ASSERT_EQ(w.deliveredB.size(), 2u);
    ASSERT_TRUE(w.deliveredB[0] == "pos0");
    ASSERT_TRUE(w.deliveredB[1] == "pos3");
    ASSERT_EQ(w.b.stats().staleSequencedDropped, 2u);
}

static void test_unreliable_passthrough() {
    WirePair w;
    w.a.setChannelMode(4, ChannelMode::Unreliable);
    w.b.setChannelMode(4, ChannelMode::Unreliable);
    ASSERT_TRUE(w.a.queueMessage(4, toBytes("fx")));
    std::vector<std::vector<uint8_t>> p;
    w.a.buildPackets(p);
    w.feedFromAToB(p[0]);
    w.feedFromAToB(p[0]);
    ASSERT_EQ(w.deliveredB.size(), 2u);
}

static void test_fragmentation_roundtrip() {
    WirePair w;
    // 5 KB message: exceeds one packet, gets fragmented transparently
    std::string big(5000, '\0');
    for (size_t i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>((i * 31 + 7) & 0xFF);

    ASSERT_TRUE(w.a.queueMessage(1, toBytes(big)));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_TRUE(packets.size() >= 4);  // 5000 / ~1174 = 5 fragments
    for (const auto& p : packets)
        ASSERT_TRUE(p.size() <= GAME_MTU);

    for (const auto& p : packets) w.feedFromAToB(p);
    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.deliveredB[0] == big);
    ASSERT_TRUE(w.b.stats().reassembliesCompleted >= 1);
}

static void test_fragmentation_out_of_order() {
    WirePair w;
    std::string big(3000, 'z');
    for (size_t i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>(i & 0xFF);

    ASSERT_TRUE(w.a.queueMessage(1, toBytes(big)));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_TRUE(packets.size() >= 3);

    // Deliver in reverse order
    for (auto it = packets.rbegin(); it != packets.rend(); ++it)
        w.feedFromAToB(*it);

    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.deliveredB[0] == big);
}

static void test_fragment_lost_then_retransmitted() {
    WirePair w;
    w.a.setResendInterval(std::chrono::milliseconds(20));
    std::string big(3000, 'q');

    ASSERT_TRUE(w.a.queueMessage(1, toBytes(big)));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_TRUE(packets.size() >= 3);

    // Drop the first packet, deliver the rest
    for (size_t i = 1; i < packets.size(); ++i)
        w.feedFromAToB(packets[i]);
    ASSERT_EQ(w.deliveredB.size(), 0u);  // incomplete

    // RTO expires: lost fragment is retransmitted
    SteadyTime later = SteadyClock::now() + std::chrono::milliseconds(100);
    std::vector<std::vector<uint8_t>> resends;
    w.a.collectRetransmits(later, resends);
    ASSERT_TRUE(!resends.empty());
    for (const auto& p : resends) w.feedFromAToB(p);

    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.deliveredB[0] == big);
}

static void test_stale_reassembly_swept() {
    WirePair w;
    w.b.setReassemblyTimeout(std::chrono::milliseconds(50));
    std::string big(3000, 's');

    ASSERT_TRUE(w.a.queueMessage(1, toBytes(big)));
    std::vector<std::vector<uint8_t>> packets;
    w.a.buildPackets(packets);
    ASSERT_TRUE(packets.size() >= 3);

    // Deliver only the first fragment; the rest are lost forever
    w.feedFromAToB(packets[0]);
    ASSERT_EQ(w.deliveredB.size(), 0u);
    uint64_t droppedBefore = w.b.stats().reassembliesDropped;

    // Nothing is retransmitted here, but the sweep runs inside collectRetransmits
    SteadyTime later = SteadyClock::now() + std::chrono::milliseconds(100);
    std::vector<std::vector<uint8_t>> resends;
    w.b.collectRetransmits(later, resends);
    ASSERT_TRUE(w.b.stats().reassembliesDropped > droppedBefore);

    // A fresh complete message afterwards is unaffected
    ASSERT_TRUE(w.a.queueMessage(1, toBytes("after")));
    w.flushAToB();
    ASSERT_EQ(w.deliveredB.size(), 1u);
    ASSERT_TRUE(w.deliveredB[0] == "after");
}

static void test_oversized_rules() {
    WirePair w;
    // Non-reliable channels reject oversized payloads (no fragmentation)
    w.a.setChannelMode(4, ChannelMode::Unreliable);
    std::vector<uint8_t> big(GAME_MAX_PAYLOAD + 1, 0xAA);
    ASSERT_TRUE(!w.a.queueMessage(4, big));
    // Reliable accepts (fragments internally)
    ASSERT_TRUE(w.a.queueMessage(1, big));
    // Absurd sizes rejected on any channel
    std::vector<uint8_t> huge(GAME_MAX_MESSAGE + 1, 0xBB);
    ASSERT_TRUE(!w.a.queueMessage(1, huge));
}

static void test_seq_wrap_comparison() {
    ASSERT_TRUE(seqGreaterThan(1, 0));
    ASSERT_TRUE(seqGreaterThan(0, 0xFFFFFFFFu));          // wrap
    ASSERT_TRUE(!seqGreaterThan(0xFFFFFFFFu, 0));
    ASSERT_TRUE(!seqGreaterThan(5, 5));
    ASSERT_TRUE(seqGreaterOrEqual(5, 5));
}

int main() {
    test_header_and_record_roundtrip();
    test_reliable_in_order_delivery();
    test_aggregation_single_packet();
    test_aggregation_splits_at_mtu();
    test_reliable_reorders_out_of_order();
    test_reliable_duplicate_dropped();
    test_retransmit_until_acked();
    test_ack_clears_pending();
    test_rtt_estimation();
    test_karn_rule_no_sample_on_resend();
    test_unreliable_sequenced_latest_wins();
    test_unreliable_passthrough();
    test_fragmentation_roundtrip();
    test_fragmentation_out_of_order();
    test_fragment_lost_then_retransmitted();
    test_stale_reassembly_swept();
    test_oversized_rules();
    test_seq_wrap_comparison();

    if (g_failures == 0) {
        std::printf("All GameConnection tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
