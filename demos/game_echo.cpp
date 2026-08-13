// OmniNet - Game networking UDP echo demo
//
// Starts a GameServer on a background thread and a GameClient in-process.
// The client sends a burst of messages on a reliable channel and a stream of
// "position updates" on an unreliable-sequenced channel; the server echoes
// everything back. Prints connection stats at the end.
#include "game/BitStream.h"
#include "game/GameClient.h"
#include "game/GameServer.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace OmniNet;
using namespace OmniNet::Game;

int main() {
    // ------------------------------------------------------------------
    // Server
    // ------------------------------------------------------------------
    GameServer server;
    auto res = server.bind("127.0.0.1", 0);  // ephemeral port
    if (!res) {
        std::fprintf(stderr, "bind failed\n");
        return 1;
    }
    const uint16_t port = server.port();
    std::printf("GameServer listening on 127.0.0.1:%u\n", port);

    server.onConnect([](ConnectionId id, const UdpEndpoint& ep) {
        std::printf("[server] client %u connected from %s\n", id, ep.toString().c_str());
    });
    server.onDisconnect([](ConnectionId id) {
        std::printf("[server] client %u disconnected\n", id);
    });
    server.onMessage([&server](ConnectionId id, uint8_t channel,
                               std::span<const uint8_t> data) {
        server.send(id, channel, data);  // echo
    });

    std::atomic<bool> stop{false};
    std::thread serverThread([&] {
        while (!stop.load())
            server.pump(2);
    });

    // ------------------------------------------------------------------
    // Client
    // ------------------------------------------------------------------
    GameClient client;
    client.setChannelMode(2, ChannelMode::UnreliableSequenced);

    std::atomic<int> echoes{0};
    client.onMessage([&](uint8_t channel, std::span<const uint8_t> data) {
        if (channel == 1) {
            BitReader r(data);
            std::printf("[client] echo: \"%s\"\n", r.readString().c_str());
        }
        ++echoes;
    });

    auto conn = client.connect("127.0.0.1", port, 3000);
    if (!conn) {
        std::fprintf(stderr, "connect failed\n");
        stop = true;
        serverThread.join();
        return 1;
    }
    std::printf("[client] connected with id %u\n", client.connectionId());

    // Reliable chat-style messages, serialized with the high-speed BitWriter
    for (int i = 0; i < 3; ++i) {
        BitWriter w;
        w.writeString("hello server, message " + std::to_string(i));
        client.send(1, w.bytes());
    }

    // Snapshot-style updates on the sequenced channel (quantized floats)
    for (int i = 0; i < 20; ++i) {
        BitWriter w;
        w.writeQuantizedFloat(static_cast<float>(i), 0.0f, 100.0f, 12);  // x
        w.writeQuantizedFloat(0.5f, 0.0f, 100.0f, 12);                    // y
        client.send(2, w.bytes());
    }

    // One large message (4 KB) — transparently fragmented and reassembled
    {
        std::string big(4096, '\0');
        for (size_t i = 0; i < big.size(); ++i)
            big[i] = static_cast<char>('A' + (i % 26));
        client.send(1, {reinterpret_cast<const uint8_t*>(big.data()), big.size()});
    }

    // Pump until all echoes arrive
    auto deadline = SteadyClock::now() + std::chrono::seconds(5);
    while (SteadyClock::now() < deadline && echoes.load() < 24)
        client.pump(5);

    std::printf("[client] received %d/24 echoes\n", echoes.load());

    const auto* connState = client.connection();
    const auto& stats = connState->stats();
    std::printf("[client] sent=%llu recv=%llu delivered=%llu retransmits=%llu fragments=%llu\n",
                static_cast<unsigned long long>(stats.packetsSent),
                static_cast<unsigned long long>(stats.packetsReceived),
                static_cast<unsigned long long>(stats.messagesDelivered),
                static_cast<unsigned long long>(stats.retransmits),
                static_cast<unsigned long long>(stats.fragmentsReceived));
    std::printf("[client] rtt=%.2fms rttvar=%.2fms rto=%lldms (samples=%llu)\n",
                connState->smoothedRttMs(), connState->rttVarMs(),
                static_cast<long long>(connState->currentRto().count()),
                static_cast<unsigned long long>(connState->rttSampleCount()));

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    stop = true;
    serverThread.join();
    return echoes.load() == 24 ? 0 : 1;
}
