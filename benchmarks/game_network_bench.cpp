// OmniNet - Game networking benchmark
// Measures: reliable/unreliable throughput (pps, Mbps) over loopback UDP,
// and RTT/retransmit behavior under simulated packet loss (in-memory).
#include "game/GameClient.h"
#include "game/GameServer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace OmniNet;
using namespace OmniNet::Game;
using Clock = std::chrono::steady_clock;

static double elapsedSec(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// ---------------------------------------------------------------------------
// 1. Loopback throughput: server echoes, client blasts for `seconds`
// ---------------------------------------------------------------------------
static void benchThroughput(const char* label, uint8_t channel,
                            ChannelMode mode, size_t payloadSize, double seconds) {
    GameServer server;
    if (!server.bind("127.0.0.1", 0)) { std::printf("bind failed\n"); return; }
    server.setChannelMode(channel, mode);
    server.onMessage([&](ConnectionId id, uint8_t ch, std::span<const uint8_t> d) {
        server.send(id, ch, d);
    });
    std::atomic<bool> stop{false};
    std::thread serverThread([&] { while (!stop.load()) server.pump(1); });

    GameClient client;
    client.setChannelMode(channel, mode);
    if (!client.connect("127.0.0.1", server.port(), 3000)) {
        std::printf("connect failed\n"); stop = true; serverThread.join(); return;
    }

    std::atomic<uint64_t> echoes{0};
    client.onMessage([&](uint8_t, std::span<const uint8_t>) { ++echoes; });

    std::vector<uint8_t> payload(payloadSize, 0xAB);
    auto t0 = Clock::now();
    uint64_t sent = 0;
    while (elapsedSec(t0) < seconds) {
        // Keep a bounded number in flight, pump in between
        for (int i = 0; i < 64; ++i) { client.send(channel, payload); ++sent; }
        client.pump(1);
    }
    double secs = elapsedSec(t0);
    client.disconnect();
    stop = true;
    serverThread.join();

    double mbps = static_cast<double>(sent * payloadSize) * 8.0 / secs / 1e6;
    std::printf("%-28s %8zu B payload: %7.0f msg/s sent, %7.0f echo/s, %7.1f Mbps payload\n",
                label, payloadSize, sent / secs, echoes.load() / secs, mbps);
}

// ---------------------------------------------------------------------------
// 2. Simulated loss: in-memory connection pair, N% packet drop
// ---------------------------------------------------------------------------
static void benchLoss(const char* label, double lossRate, int messages) {
    GameConnection a{{"10.0.0.1", 1}, 1};
    GameConnection b{{"10.0.0.2", 2}, 2};
    a.setChannelMode(1, ChannelMode::Reliable);
    b.setChannelMode(1, ChannelMode::Reliable);
    a.setResendInterval(std::chrono::milliseconds(20));

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    auto drop = [&] { return dist(rng) < lossRate; };

    uint64_t delivered = 0;
    std::vector<uint8_t> payload(200, 0xCD);

    auto t0 = Clock::now();
    // Simulate: each "tick" is 5ms of virtual time
    int64_t virtualNowMs = 0;
    int queuedMsg = 0;
    SteadyTime simBase = Clock::now();
    while (delivered < static_cast<uint64_t>(messages)) {
        // queue up to `messages` total
        while (queuedMsg < messages && a.stats().messagesSent - b.stats().messagesDelivered < 32) {
            a.queueMessage(1, payload);
            ++queuedMsg;
        }
        std::vector<std::vector<uint8_t>> packets;
        a.buildPackets(packets);
        a.collectRetransmits(simBase + std::chrono::milliseconds(virtualNowMs), packets);
        for (const auto& p : packets) {
            if (drop()) continue;
            b.processPacket(p, [&](uint8_t, std::span<const uint8_t>) { ++delivered; });
        }
        // b replies (acks piggyback)
        std::vector<std::vector<uint8_t>> back;
        b.queueMessage(1, payload);  // keeps acks flowing
        b.buildPackets(back);
        for (const auto& p : back) {
            if (drop()) continue;
            a.processPacket(p, [](uint8_t, std::span<const uint8_t>) {});
        }
        virtualNowMs += 5;
    }
    double secs = elapsedSec(t0);

    const auto& sa = a.stats();
    std::printf("%-28s loss %4.1f%%: %6d msgs in %6.2fs (%6.0f msg/s), retransmits=%llu, srtt=%.2fms rto=%lldms\n",
                label, lossRate * 100.0, messages, secs, messages / secs,
                static_cast<unsigned long long>(sa.retransmits),
                a.smoothedRttMs(),
                static_cast<long long>(a.currentRto().count()));
}

int main() {
    std::printf("=== OmniNet game networking benchmark ===\n\n");
    std::printf("-- Loopback throughput (2s per case) --\n");
    benchThroughput("reliable small", 1, ChannelMode::Reliable, 64, 2.0);
    benchThroughput("reliable 1KB", 1, ChannelMode::Reliable, 1024, 2.0);
    benchThroughput("unreliable-sequenced", 2, ChannelMode::UnreliableSequenced, 64, 2.0);

    std::printf("\n-- Simulated loss (in-memory, 2000 msgs, 200 B) --\n");
    benchLoss("clean", 0.0, 2000);
    benchLoss("lossy", 0.05, 2000);
    benchLoss("very lossy", 0.15, 2000);
    return 0;
}
