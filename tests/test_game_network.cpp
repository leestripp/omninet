// OmniNet - GameServer/GameClient end-to-end tests over loopback UDP
#include "crypto/Crypto.h"
#include "game/GameClient.h"
#include "game/GameServer.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace OmniNet;
using namespace OmniNet::Game;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

namespace {

// Run a server pump loop on a background thread until stop is set.
struct ServerRunner {
    GameServer server;
    std::atomic<bool> stop{false};
    std::thread thread;

    std::vector<std::string> received;
    std::atomic<int> connects{0};
    std::atomic<int> disconnects{0};

    bool start(uint16_t port, const std::string& addr = "127.0.0.1") {
        auto res = server.bind(addr, port);
        if (!res) return false;

        server.onConnect([this](ConnectionId, const UdpEndpoint&) { ++connects; });
        server.onDisconnect([this](ConnectionId) { ++disconnects; });
        server.onMessage([this](ConnectionId id, uint8_t channel,
                                std::span<const uint8_t> data) {
            received.emplace_back(reinterpret_cast<const char*>(data.data()), data.size());
            server.send(id, channel, data);  // echo back
        });

        thread = std::thread([this] {
            while (!stop.load())
                server.pump(2);
        });
        return true;
    }

    void shutdown() {
        stop = true;
        if (thread.joinable()) thread.join();
        server.close();
    }

    ~ServerRunner() { shutdown(); }
};

std::vector<uint8_t> toBytes(const std::string& s) { return {s.begin(), s.end()}; }

// Write a self-signed cert+key pair for DTLS tests; returns paths
std::pair<std::string, std::string> makeTestCert(const char* tag) {
    auto [certPem, keyPem] = Crypto::generateSelfSignedCert(tag, 1);
    std::string certPath = std::string("/tmp/gamenet_") + tag + "_cert.pem";
    std::string keyPath = std::string("/tmp/gamenet_") + tag + "_key.pem";
    {
        std::ofstream f(certPath, std::ios::binary);
        f.write(reinterpret_cast<const char*>(certPem.data()), certPem.size());
    }
    {
        std::ofstream f(keyPath, std::ios::binary);
        f.write(reinterpret_cast<const char*>(keyPem.data()), keyPem.size());
    }
    return {certPath, keyPath};
}

// Pump the client until predicate is true or timeout expires
template <typename Pred>
bool pumpUntil(GameClient& client, Pred pred, int timeoutMs = 5000) {
    auto deadline = SteadyClock::now() + std::chrono::milliseconds(timeoutMs);
    while (SteadyClock::now() < deadline) {
        client.pump(5);
        if (pred()) return true;
    }
    return pred();
}

} // namespace

static void test_connect_echo_disconnect() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    GameClient client;
    std::vector<std::string> replies;
    std::atomic<bool> disconnected{false};
    client.onMessage([&](uint8_t, std::span<const uint8_t> d) {
        replies.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
    });
    client.onDisconnect([&] { disconnected = true; });

    auto res = client.connect("127.0.0.1", runner.server.port(), 3000);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(client.isConnected());
    ASSERT_TRUE(client.connectionId() != 0);

    // Reliable echo round trip
    ASSERT_TRUE(client.send(1, toBytes("ping")));
    ASSERT_TRUE(pumpUntil(client, [&] { return !replies.empty(); }));
    ASSERT_EQ(replies.size(), 1u);
    ASSERT_TRUE(replies[0] == "ping");

    // Give the server a moment to register the connection
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(runner.connects.load() >= 1);
    ASSERT_EQ(runner.received.size(), 1u);
    ASSERT_TRUE(runner.received[0] == "ping");

    client.disconnect();
    ASSERT_TRUE(!client.isConnected());

    // Server should notice the graceful disconnect
    auto deadline = SteadyClock::now() + std::chrono::milliseconds(2000);
    while (SteadyClock::now() < deadline && runner.disconnects.load() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(runner.disconnects.load(), 1);
    ASSERT_TRUE(!disconnected.load());  // client-initiated: no callback
}

static void test_ordered_reliable_stream() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    GameClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", runner.server.port(), 3000).has_value());

    // Blast 50 reliable messages; echo replies must come back exactly once each
    for (int i = 0; i < 50; ++i)
        ASSERT_TRUE(client.send(1, toBytes("m" + std::to_string(i))));

    std::vector<std::string> replies;
    client.onMessage([&](uint8_t, std::span<const uint8_t> d) {
        replies.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
    });

    ASSERT_TRUE(pumpUntil(client, [&] { return replies.size() >= 50; }, 10000));
    ASSERT_EQ(replies.size(), 50u);
    for (int i = 0; i < 50; ++i)
        ASSERT_TRUE(replies[i] == "m" + std::to_string(i));
}

static void test_unreliable_sequenced_channel() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    GameClient client;
    client.setChannelMode(2, ChannelMode::UnreliableSequenced);
    ASSERT_TRUE(client.connect("127.0.0.1", runner.server.port(), 3000).has_value());

    std::atomic<int> replies{0};
    client.onMessage([&](uint8_t channel, std::span<const uint8_t>) {
        ASSERT_EQ(channel, 2);
        ++replies;
    });

    // Position-update style burst on the sequenced channel
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(client.send(2, toBytes("pos")));

    ASSERT_TRUE(pumpUntil(client, [&] { return replies.load() >= 10; }, 10000));
}

static void test_server_full_denies() {
    ServerRunner runner;
    runner.server.setMaxClients(1);
    ASSERT_TRUE(runner.start(0));

    GameClient first;
    ASSERT_TRUE(first.connect("127.0.0.1", runner.server.port(), 3000).has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    GameClient second;
    auto res = second.connect("127.0.0.1", runner.server.port(), 1500);
    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error() == SocketError::ConnectionRefused);
}

static void test_multiple_clients_broadcast() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    GameClient c1, c2;
    ASSERT_TRUE(c1.connect("127.0.0.1", runner.server.port(), 3000).has_value());
    ASSERT_TRUE(c2.connect("127.0.0.1", runner.server.port(), 3000).has_value());

    auto deadline = SteadyClock::now() + std::chrono::milliseconds(2000);
    while (SteadyClock::now() < deadline && runner.server.clientCount() < 2)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(runner.server.clientCount(), 2u);

    std::atomic<int> got1{0}, got2{0};
    c1.onMessage([&](uint8_t, std::span<const uint8_t>) { ++got1; });
    c2.onMessage([&](uint8_t, std::span<const uint8_t>) { ++got2; });

    runner.server.broadcast(1, toBytes("hello all"));

    ASSERT_TRUE(pumpUntil(c1, [&] { return got1.load() >= 1; }));
    ASSERT_TRUE(pumpUntil(c2, [&] { return got2.load() >= 1; }));
}

static void test_connect_flood_hardening() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    // A spoofed-style flood: raw UDP bare Connects, never answering challenges
    UdpSocket flooder;
    ASSERT_TRUE(flooder.open().has_value());
    uint8_t connectRec[PACKET_HEADER_SIZE + MESSAGE_HEADER_SIZE + 3]{};
    PacketHeader ph{};  // connId 0, seq 0
    encodePacketHeader(connectRec, ph);
    std::vector<uint8_t> pkt(connectRec, connectRec + PACKET_HEADER_SIZE);
    uint8_t payload[3] = {SystemMsg::Connect,
                          static_cast<uint8_t>(GAME_PROTOCOL_VERSION & 0xFF),
                          static_cast<uint8_t>(GAME_PROTOCOL_VERSION >> 8)};
    encodeRecord(pkt, SYSTEM_CHANNEL, 0, 0, 0, 0, payload);

    for (int i = 0; i < 200; ++i)
        (void)flooder.sendTo(pkt, {"127.0.0.1", runner.server.port()});

    // Let the server chew through the flood
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // No client was ever committed, no onConnect fired
    ASSERT_EQ(runner.server.clientCount(), 0u);
    ASSERT_EQ(runner.connects.load(), 0);

    // A legit client still connects immediately (challenge-response)
    GameClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", runner.server.port(), 3000).has_value());
    ASSERT_TRUE(client.isConnected());
}

static void test_ipv6_end_to_end() {
    // Skip gracefully when the host has no IPv6 stack
    {
        UdpSocket probe;
        if (!probe.bind("::1", 0).has_value()) {
            std::printf("SKIP ipv6 e2e (no IPv6 stack)\n");
            return;
        }
    }

    ServerRunner runner;
    ASSERT_TRUE(runner.start(0, "::1"));

    GameClient client;
    std::vector<std::string> replies;
    client.onMessage([&](uint8_t, std::span<const uint8_t> d) {
        replies.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
    });
    ASSERT_TRUE(client.connect("::1", runner.server.port(), 3000).has_value());
    ASSERT_TRUE(client.isConnected());

    ASSERT_TRUE(client.send(1, toBytes("ipv6 game traffic")));
    ASSERT_TRUE(pumpUntil(client, [&] { return !replies.empty(); }));
    ASSERT_TRUE(replies[0] == "ipv6 game traffic");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(runner.connects.load(), 1);
}

static void test_connection_migration() {
    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));

    GameClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", runner.server.port(), 3000).has_value());
    ConnectionId id = client.connectionId();
    ASSERT_TRUE(id != 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto sendPingFrom = [&](UdpSocket& sock) {
        std::vector<uint8_t> pkt(PACKET_HEADER_SIZE);
        PacketHeader ph{};
        ph.connectionId = id;
        encodePacketHeader(pkt.data(), ph);
        uint8_t ping[1] = {SystemMsg::Ping};
        encodeRecord(pkt, SYSTEM_CHANNEL, 0, 0, 0, 0, ping);
        return sock.sendTo(pkt, {"127.0.0.1", runner.server.port()}).has_value();
    };

    const GameConnection* conn = runner.server.client(id);
    ASSERT_TRUE(conn != nullptr);
    uint16_t originalPort = conn->remote().port;
    uint64_t receivedBefore = conn->stats().packetsReceived;

    // NAT rebind: keepalive with the client's connId from a new source port
    UdpSocket movedB;
    ASSERT_TRUE(movedB.open().has_value());
    ASSERT_TRUE(sendPingFrom(movedB));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(conn->remote().port, movedB.localPort());
    ASSERT_TRUE(conn->remote().port != originalPort);

    // Migration is repeatable: another rebind moves it again
    UdpSocket movedC;
    ASSERT_TRUE(movedC.open().has_value());
    ASSERT_TRUE(sendPingFrom(movedC));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(conn->remote().port, movedC.localPort());

    // The connection survived both migrations: still tracked, no disconnect,
    // and the migrated packets were processed
    ASSERT_EQ(runner.server.clientCount(), 1u);
    ASSERT_EQ(runner.disconnects.load(), 0);
    ASSERT_TRUE(conn->stats().packetsReceived >= receivedBefore + 2);

    // A packet from a GUESSED connId without a keepalive does not migrate
    UdpSocket attacker;
    ASSERT_TRUE(attacker.open().has_value());
    std::vector<uint8_t> pkt(PACKET_HEADER_SIZE);
    PacketHeader ph{};
    ph.connectionId = id;
    encodePacketHeader(pkt.data(), ph);
    uint8_t msg[4] = {'n', 'o', 'p', 'e'};
    encodeRecord(pkt, 1, 0, 0, 0, 0, msg);  // app channel, no Ping
    ASSERT_TRUE(attacker.sendTo(pkt, {"127.0.0.1", runner.server.port()}).has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(conn->remote().port, movedC.localPort());  // unchanged
}

static void run_dtls_echo_case(bool requireCookie) {
    auto [certPath, keyPath] = makeTestCert(requireCookie ? "dtls_c" : "dtls_p");

    ServerRunner runner;
    ASSERT_TRUE(runner.start(0));
    runner.server.enableDtls(certPath, keyPath, requireCookie);

    GameClient client;
    std::vector<std::string> replies;
    client.onMessage([&](uint8_t, std::span<const uint8_t> d) {
        replies.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
    });
    auto res = client.connectDtls("127.0.0.1", runner.server.port(), 5000);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(client.isConnected());

    ASSERT_TRUE(client.send(1, toBytes("encrypted ping")));
    ASSERT_TRUE(pumpUntil(client, [&] { return !replies.empty(); }));
    ASSERT_TRUE(replies[0] == "encrypted ping");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(runner.connects.load(), 1);
}

static void test_dtls_echo() { run_dtls_echo_case(false); }
static void test_dtls_cookie_echo() { run_dtls_echo_case(true); }

static void test_connect_timeout() {
    GameClient client;
    // Nothing is listening on this port; connect must time out
    auto res = client.connect("127.0.0.1", 59999, 600);
    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error() == SocketError::Timeout);
    ASSERT_TRUE(!client.isConnected());
}

int main() {
    test_connect_echo_disconnect();
    test_ordered_reliable_stream();
    test_unreliable_sequenced_channel();
    test_server_full_denies();
    test_multiple_clients_broadcast();
    test_connect_flood_hardening();
    test_ipv6_end_to_end();
    test_connection_migration();
    test_dtls_echo();
    test_dtls_cookie_echo();
    test_connect_timeout();

    if (g_failures == 0) {
        std::printf("All GameNetwork tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
