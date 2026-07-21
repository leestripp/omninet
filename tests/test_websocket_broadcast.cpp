// OmniNet - WebSocket broadcast / fan-out tests (v0.7.0 Phase 11b)
//
// Tests WebSocket::Server client tracking + broadcast API using a
// MockConnection that overrides the now-virtual sendText/isOpen.
#include <atomic>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "websocket/WebSocket.h"

using namespace OmniNet;
using namespace OmniNet::WebSocket;

// ============================================================================
// Mock Connection
// ============================================================================

class MockConnection : public Connection {
public:
    MockConnection()
        : Connection(std::unique_ptr<Socket>(nullptr), std::string_view("/mock")),
          m_sendCount(0), m_failOnSend(false), m_mockOpen(true) {}

    void sendText(const std::string& message) override {
        if (m_failOnSend) {
            throw std::runtime_error("mock send failure");
        }
        ++m_sendCount;
        m_receivedMessages.push_back(message);
    }

    bool isOpen() const override { return m_mockOpen; }

    void setOpen(bool open) { m_mockOpen = open; }
    void setFailOnSend(bool fail) { m_failOnSend = fail; }
    int sendCount() const { return m_sendCount.load(); }
    const std::vector<std::string>& receivedMessages() const { return m_receivedMessages; }

private:
    std::atomic<int> m_sendCount;
    bool m_failOnSend;
    bool m_mockOpen;
    std::vector<std::string> m_receivedMessages;
};

// ============================================================================
// Minimal test framework
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

static void run_test(const char* name, void (*func)()) {
    std::cout << "  " << name << "... ";
    try {
        func();
        std::cout << "PASS" << std::endl;
        ++g_passed;
    } catch (const std::exception& e) {
        std::cout << "FAIL (" << e.what() << ")" << std::endl;
        ++g_failed;
    } catch (...) {
        std::cout << "FAIL (unknown)" << std::endl;
        ++g_failed;
    }
}

#define TEST(name) \
    static void test_##name(); \
    static struct TestRegistrar_##name { \
        TestRegistrar_##name() { run_test(#name, test_##name); } \
    } g_registrar_##name; \
    static void test_##name()

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { throw std::runtime_error("ASSERT_TRUE failed: " #cond); } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::ostringstream oss; \
        oss << "ASSERT_EQ failed: " #a << " (" << (a) << ") != " #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

// ============================================================================
// Tests
// ============================================================================

TEST(add_remove_client) {
    Server server;
    MockConnection c1, c2;
    auto id1 = server.addClient(c1, "/ws");
    auto id2 = server.addClient(c2, "/ws");
    ASSERT_TRUE(id1 != 0);
    ASSERT_TRUE(id2 != 0);
    ASSERT_TRUE(id1 != id2);
    ASSERT_EQ(server.connectionCount(), 2u);
    server.removeClient(id1);
    ASSERT_EQ(server.connectionCount(), 1u);
    server.removeClient(id2);
    ASSERT_EQ(server.connectionCount(), 0u);
}

TEST(connection_count) {
    Server server;
    ASSERT_EQ(server.connectionCount(), 0u);
    MockConnection a, b, c;
    server.addClient(a, "/x");
    server.addClient(b, "/y");
    server.addClient(c, "/x");
    ASSERT_EQ(server.connectionCount(), 3u);
    ASSERT_EQ(server.connectionCount("/x"), 2u);
    ASSERT_EQ(server.connectionCount("/y"), 1u);
    ASSERT_EQ(server.connectionCount("/z"), 0u);
}

TEST(broadcast_basic) {
    Server server;
    MockConnection a, b, c;
    server.addClient(a, "/ws");
    server.addClient(b, "/ws");
    server.addClient(c, "/ws");
    server.broadcast("hello");
    ASSERT_EQ(a.sendCount(), 1);
    ASSERT_EQ(b.sendCount(), 1);
    ASSERT_EQ(c.sendCount(), 1);
    ASSERT_EQ(a.receivedMessages()[0], std::string("hello"));
}

TEST(broadcast_by_path) {
    Server server;
    MockConnection a, b, c;
    server.addClient(a, "/x");
    server.addClient(b, "/x");
    server.addClient(c, "/y");
    server.broadcast("only-x", "/x");
    ASSERT_EQ(a.sendCount(), 1);
    ASSERT_EQ(b.sendCount(), 1);
    ASSERT_EQ(c.sendCount(), 0);
}

TEST(broadcast_one_dead) {
    Server server;
    MockConnection healthy1, dead, healthy2;
    server.addClient(healthy1, "/ws");
    auto idDead = server.addClient(dead, "/ws");
    server.addClient(healthy2, "/ws");
    dead.setFailOnSend(true);
    ASSERT_EQ(server.connectionCount(), 3u);
    server.broadcast("go");
    ASSERT_EQ(healthy1.sendCount(), 1);
    ASSERT_EQ(healthy2.sendCount(), 1);
    ASSERT_EQ(server.connectionCount(), 2u);
    (void)idDead;
}

TEST(clients_snapshot) {
    Server server;
    MockConnection a, b;
    auto idA = server.addClient(a, "/p");
    auto idB = server.addClient(b, "/p");
    auto snap = server.clients();
    ASSERT_EQ(snap.size(), 2u);
    bool hasA = false, hasB = false;
    for (auto id : snap) {
        if (id == idA) hasA = true;
        if (id == idB) hasB = true;
    }
    ASSERT_TRUE(hasA);
    ASSERT_TRUE(hasB);
    server.removeClient(idA);
    ASSERT_EQ(snap.size(), 2u);
    ASSERT_EQ(server.connectionCount(), 1u);
}

TEST(send_to_unknown_id) {
    Server server;
    MockConnection a;
    server.addClient(a, "/ws");
    server.sendTo(99999, "nope");
    ASSERT_EQ(a.sendCount(), 0);
    ASSERT_EQ(server.connectionCount(), 1u);
}

TEST(send_to_specific) {
    Server server;
    MockConnection a, b;
    auto idA = server.addClient(a, "/ws");
    server.addClient(b, "/ws");
    server.sendTo(idA, "only-a");
    ASSERT_EQ(a.sendCount(), 1);
    ASSERT_EQ(b.sendCount(), 0);
    ASSERT_EQ(a.receivedMessages()[0], std::string("only-a"));
}

TEST(remove_unknown_id) {
    Server server;
    MockConnection a;
    server.addClient(a, "/ws");
    server.removeClient(99999);
    ASSERT_EQ(server.connectionCount(), 1u);
}

TEST(broadcast_text_alias) {
    Server server;
    MockConnection a;
    server.addClient(a, "/ws");
    server.broadcastText("via-alias");
    ASSERT_EQ(a.receivedMessages()[0], std::string("via-alias"));
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "WebSocket broadcast tests (v0.7.0 Phase 11b)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
