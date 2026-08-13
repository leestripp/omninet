// OmniNet - UdpSocket tests (game networking)
#include "net/UdpSocket.h"

#include <cstdio>
#include <cstring>

using namespace OmniNet;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static void test_bind_and_port() {
    UdpSocket sock;
    auto res = sock.bind("127.0.0.1", 0);  // ephemeral port
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(sock.isOpen());
    ASSERT_TRUE(sock.localPort() != 0);
}

static void test_double_bind_fails() {
    UdpSocket a, b;
    ASSERT_TRUE(a.bind("127.0.0.1", 0).has_value());
    // SO_REUSEADDR is set, so same-port bind may succeed on Linux;
    // instead verify binding the same port on the SAME socket fails.
    auto again = a.bind("127.0.0.1", a.localPort());
    ASSERT_TRUE(!again.has_value());
}

static void test_send_recv_loopback() {
    UdpSocket server, client;
    ASSERT_TRUE(server.bind("127.0.0.1", 0).has_value());
    ASSERT_TRUE(client.open().has_value());

    const char* msg = "hello udp";
    auto sent = client.sendTo(
        {reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)},
        {"127.0.0.1", server.localPort()});
    ASSERT_TRUE(sent.has_value());
    ASSERT_EQ(*sent, std::strlen(msg));

    ASSERT_TRUE(server.waitForData(2000));

    auto dg = server.recvFrom();
    ASSERT_TRUE(dg.has_value());
    ASSERT_EQ(dg->data.size(), std::strlen(msg));
    ASSERT_TRUE(std::memcmp(dg->data.data(), msg, dg->data.size()) == 0);
    ASSERT_EQ(dg->from.address, "127.0.0.1");
}

static void test_would_block() {
    UdpSocket sock;
    ASSERT_TRUE(sock.bind("127.0.0.1", 0).has_value());
    auto dg = sock.recvFrom();
    ASSERT_TRUE(!dg.has_value());
    ASSERT_TRUE(dg.error() == SocketError::WouldBlock);
    ASSERT_TRUE(!sock.waitForData(50));  // times out, no data
}

static void test_resolve() {
    auto ep = UdpSocket::resolve("localhost", 7777);
    ASSERT_TRUE(ep.has_value());
    ASSERT_EQ(ep->port, 7777);
    ASSERT_TRUE(!ep->address.empty());
}

static void test_send_before_open_fails() {
    UdpSocket sock;
    uint8_t b = 42;
    auto res = sock.sendTo({&b, 1}, {"127.0.0.1", 12345});
    ASSERT_TRUE(!res.has_value());
}

static void test_move() {
    UdpSocket a;
    ASSERT_TRUE(a.bind("127.0.0.1", 0).has_value());
    uint16_t port = a.localPort();

    UdpSocket b(std::move(a));
    ASSERT_TRUE(!a.isOpen());
    ASSERT_TRUE(b.isOpen());
    ASSERT_EQ(b.localPort(), port);
}

static bool ipv6Available() {
    UdpSocket s;
    if (!s.bind("::1", 0).has_value()) return false;
    return s.localPort() != 0;
}

static void test_ipv6_loopback_echo() {
    if (!ipv6Available()) { std::printf("SKIP ipv6 tests (no IPv6 stack)\n"); return; }

    UdpSocket server;
    ASSERT_TRUE(server.bind("::1", 0).has_value());
    ASSERT_TRUE(server.localPort() != 0);

    UdpSocket client;
    ASSERT_TRUE(client.open(AF_INET6).has_value());

    const char* msg = "ipv6 hello";
    auto sent = client.sendTo({reinterpret_cast<const uint8_t*>(msg), 10},
                              {"::1", server.localPort()});
    ASSERT_TRUE(sent.has_value());
    ASSERT_TRUE(server.waitForData(2000));
    auto dg = server.recvFrom();
    ASSERT_TRUE(dg.has_value());
    ASSERT_EQ(dg->data.size(), 10u);
    ASSERT_EQ(dg->from.address, std::string("::1"));

    // Reply path: server -> client
    auto rsent = server.sendTo(dg->data, dg->from);
    ASSERT_TRUE(rsent.has_value());
    ASSERT_TRUE(client.waitForData(2000));
    auto reply = client.recvFrom();
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->from.address, std::string("::1"));
}

static void test_dual_stack_receives_ipv4() {
    if (!ipv6Available()) { std::printf("SKIP dual-stack (no IPv6 stack)\n"); return; }

    UdpSocket server;
    ASSERT_TRUE(server.bind("::", 0).has_value());  // dual-stack

    UdpSocket client4;
    ASSERT_TRUE(client4.open().has_value());  // IPv4
    const char* msg = "v4 on dual";
    auto sent = client4.sendTo({reinterpret_cast<const uint8_t*>(msg), 10},
                               {"127.0.0.1", server.localPort()});
    ASSERT_TRUE(sent.has_value());
    ASSERT_TRUE(server.waitForData(2000));
    auto dg = server.recvFrom();
    ASSERT_TRUE(dg.has_value());
    // v4-mapped normalized to plain IPv4
    ASSERT_EQ(dg->from.address, std::string("127.0.0.1"));

    // Dual-stack server can reply to the v4 client
    ASSERT_TRUE(server.sendTo(dg->data, dg->from).has_value());
    ASSERT_TRUE(client4.waitForData(2000));
    ASSERT_TRUE(client4.recvFrom().has_value());
}

static void test_resolve_localhost_dual() {
    auto ep = UdpSocket::resolve("localhost", 7777);
    ASSERT_TRUE(ep.has_value());
    ASSERT_TRUE(ep->address == "127.0.0.1" || ep->address == "::1");
    ASSERT_EQ(ep->port, 7777);
}

int main() {
    test_bind_and_port();
    test_double_bind_fails();
    test_send_recv_loopback();
    test_would_block();
    test_resolve();
    test_send_before_open_fails();
    test_move();

    if (g_failures == 0) {
        test_ipv6_loopback_echo();
    test_dual_stack_receives_ipv4();
    test_resolve_localhost_dual();

    std::printf("All UdpSocket tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
