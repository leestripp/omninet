// OmniNet - WebSocket end-to-end echo test (v0.7.0, prompt.txt)
//
// Verifies the full HTTP→WebSocket upgrade handoff works:
//  1. WebSocket::Server::on() registers the upgrade route
//  2. Client sends valid upgrade request
//  3. Server responds 101 Switching Protocols (NO Content-Length)
//  4. Connection is handed off to the per-path handler
//  5. addClient() is called (connectionCount() > 0)
//  6. Text frames round-trip (echo)
//  7. broadcast() reaches all connected clients
//  8. removeClient() works (connectionCount() drops on disconnect)
//
// This is the regression test for the 3 critical bugs filed in
// prompt.txt (Content-Length in 101, handler never called,
// addClient never called).

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "websocket/WebSocket.h"
#include "http/HttpServer.h"

using namespace OmniNet;
using namespace OmniNet::WebSocket;
using namespace std::chrono_literals;

// ============================================================================
// Minimal test framework (matches the other tests/ style)
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
    if (!(cond)) { \
        throw std::runtime_error("ASSERT_TRUE failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        std::ostringstream oss; \
        oss << "ASSERT_EQ failed: " #a " (" << _a \
            << ") != " #b " (" << _b << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

struct JoinableThread {
    std::thread t;
    template<typename F>
    explicit JoinableThread(F&& f) : t(std::forward<F>(f)) {}
    ~JoinableThread() { if (t.joinable()) t.join(); }
};

// ============================================================================
// TCP helpers
// ============================================================================

static int tcp_connect(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

static int send_all(int fd, const void* data, size_t n) {
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < n) {
        ssize_t k = ::send(fd, p + sent, n - sent, MSG_NOSIGNAL);
        if (k <= 0) return -1;
        sent += static_cast<size_t>(k);
    }
    return 0;
}

static int recv_until(int fd, std::string& out, const std::string& delim,
                      int timeout_ms = 2000) {
    char buf[4096];
    while (out.find(delim) == std::string::npos) {
        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int sel = ::select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (sel <= 0) return -1;  // timeout or error
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return -1;
        out.append(buf, static_cast<size_t>(n));
    }
    return 0;
}

// ============================================================================
// WebSocket frame helpers (RFC 6455)
// ============================================================================

// Build a masked text frame (client→server: must be masked).
// maskKey[0..3] is the 4-byte masking key.
static std::vector<uint8_t> make_client_text_frame(
    const std::string& text, const uint8_t maskKey[4]) {
    std::vector<uint8_t> out;
    out.push_back(0x81);  // FIN=1, opcode=1 (text)
    size_t len = text.size();
    if (len < 126) {
        out.push_back(0x80 | static_cast<uint8_t>(len));  // MASK=1, len
    } else if (len <= 0xFFFF) {
        out.push_back(0x80 | 126);
        out.push_back(static_cast<uint8_t>(len >> 8));
        out.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        out.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i)
            out.push_back(static_cast<uint8_t>(len >> (i * 8)));
    }
    out.push_back(maskKey[0]);
    out.push_back(maskKey[1]);
    out.push_back(maskKey[2]);
    out.push_back(maskKey[3]);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(static_cast<uint8_t>(text[i]) ^ maskKey[i % 4]);
    }
    return out;
}

// Parse a single unmasked server frame. Returns the payload as a string.
// Throws on protocol error.
static std::string parse_server_text_frame(const std::vector<uint8_t>& raw) {
    ASSERT_TRUE(raw.size() >= 2);
    uint8_t b0 = raw[0];
    uint8_t b1 = raw[1];
    bool fin = (b0 & 0x80) != 0;
    uint8_t opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    ASSERT_TRUE(!masked);  // server→client must NOT be masked
    ASSERT_TRUE(fin);
    ASSERT_EQ(static_cast<int>(opcode), 1);  // text
    uint64_t len = b1 & 0x7F;
    size_t offset = 2;
    if (len == 126) {
        ASSERT_TRUE(raw.size() >= offset + 2);
        len = (static_cast<uint16_t>(raw[offset]) << 8) | raw[offset + 1];
        offset += 2;
    } else if (len == 127) {
        ASSERT_TRUE(raw.size() >= offset + 8);
        len = 0;
        for (int i = 0; i < 8; ++i)
            len = (len << 8) | raw[offset + i];
        offset += 8;
    }
    ASSERT_TRUE(raw.size() >= offset + len);
    return std::string(raw.begin() + offset, raw.begin() + offset + len);
}

// Read a full server frame with a timeout. Returns payload as string.
static std::string read_server_text_frame(int fd, int timeout_ms = 2000) {
    std::vector<uint8_t> raw;
    // First, read the 2-byte header
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int sel = ::select(fd + 1, &fds, nullptr, nullptr, &tv);
    ASSERT_TRUE(sel > 0);
    char hdr[2];
    ssize_t n = ::recv(fd, hdr, 2, 0);
    ASSERT_EQ(n, 2);
    raw.push_back(static_cast<uint8_t>(hdr[0]));
    raw.push_back(static_cast<uint8_t>(hdr[1]));

    uint64_t len = hdr[1] & 0x7F;
    size_t extra = 0;
    if (len == 126) extra = 2;
    else if (len == 127) extra = 8;

    if (extra > 0) {
        size_t got = 0;
        while (got < extra) {
            char buf[8];
            n = ::recv(fd, buf, extra - got, 0);
            ASSERT_TRUE(n > 0);
            for (ssize_t i = 0; i < n; ++i) raw.push_back(static_cast<uint8_t>(buf[i]));
            got += static_cast<size_t>(n);
        }
        if (len == 126) {
            len = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];
        } else {
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | raw[2 + i];
        }
    }

    size_t got = 0;
    while (got < len) {
        char buf[4096];
        size_t want = static_cast<size_t>(len) - got;
        if (want > sizeof(buf)) want = sizeof(buf);
        n = ::recv(fd, buf, want, 0);
        ASSERT_TRUE(n > 0);
        for (ssize_t i = 0; i < n; ++i) raw.push_back(static_cast<uint8_t>(buf[i]));
        got += static_cast<size_t>(n);
    }

    return parse_server_text_frame(raw);
}

// ============================================================================
// Tests
// ============================================================================

TEST(handshake_returns_101_no_content_length) {
    // Bug #1: verify the 101 response has NO Content-Length header
    // (RFC 6455 §1.3, RFC 7230 §3.3.2).
    OmniNet::HTTP::Server::Config cfg;
    cfg.port = 19101;
    cfg.workerCount = 1;
    OmniNet::HTTP::Server server(cfg);

    // Use the HTTP server directly to validate the 101 path
    // (we don't need a full WebSocket::Server for this test).
    server.router().any("/ws", [](const OmniNet::HTTP::Request& req,
                                  OmniNet::HTTP::Response& resp) {
        resp.setStatus(static_cast<OmniNet::HTTP::StatusCode>(101));
        resp.setHeader("Upgrade", std::string("websocket"));
        resp.setHeader("Connection", std::string("Upgrade"));
        resp.setHeader("Sec-WebSocket-Accept", std::string("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
        resp.markProtocolSwitch();
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(19101);
    ASSERT_TRUE(fd >= 0);

    const std::string req =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_EQ(send_all(fd, req.data(), req.size()), 0);

    std::string resp;
    ASSERT_EQ(recv_until(fd, resp, "\r\n\r\n", 2000), 0);

    // Must be 101
    ASSERT_TRUE(resp.find("101 Switching Protocols") != std::string::npos);
    // Must NOT have Content-Length (prompt.txt Bug #1)
    ASSERT_TRUE(resp.find("Content-Length") == std::string::npos);
    // Must have the right Upgrade headers
    ASSERT_TRUE(resp.find("Upgrade: websocket") != std::string::npos);
    ASSERT_TRUE(resp.find("Connection: Upgrade") != std::string::npos);

    ::close(fd);
    server.stop();
}

TEST(echo_round_trip) {
    // Bug #2 + #3: verify the per-path handler is called and
    // addClient() registers the connection (connectionCount() > 0).
    OmniNet::HTTP::Server::Config cfg;
    cfg.port = 19102;
    cfg.workerCount = 1;
    WebSocket::Server ws(cfg);

    std::atomic<int> handlerCalls{0};
    std::atomic<int> echoCount{0};

    ws.on("/echo", [&](Connection& conn) {
        handlerCalls++;
        conn.onText([&](std::string msg) {
            echoCount++;
            conn.sendText("echo: " + msg);
        });
        conn.onClose([](CloseCode, std::string) {
            // client closed
        });
        // Block until the connection closes (v0.7.0: readLoop)
        conn.readLoop();
    });

    JoinableThread t([&]() { ws.start(); });
    std::this_thread::sleep_for(200ms);

    // Verify the connection is registered
    ASSERT_EQ(ws.connectionCount("/echo"), size_t{0});

    int fd = tcp_connect(19102);
    ASSERT_TRUE(fd >= 0);

    const std::string req =
        "GET /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_EQ(send_all(fd, req.data(), req.size()), 0);

    std::string resp;
    ASSERT_EQ(recv_until(fd, resp, "\r\n\r\n", 2000), 0);
    ASSERT_TRUE(resp.find("101 Switching Protocols") != std::string::npos);
    ASSERT_TRUE(resp.find("Content-Length") == std::string::npos);

    // Give the server a moment to register the client and call
    // the handler (the post-upgrade callback runs in the HTTP
    // server's connection thread).
    std::this_thread::sleep_for(100ms);

    // Bug #3 fix: addClient was called automatically
    ASSERT_TRUE(ws.connectionCount("/echo") >= size_t{1});
    // Bug #2 fix: the handler was called
    ASSERT_TRUE(handlerCalls.load() >= 1);

    // Send a masked text frame with "hello"
    uint8_t mask[4] = {0x01, 0x02, 0x03, 0x04};
    auto frame = make_client_text_frame("hello", mask);
    ASSERT_EQ(send_all(fd, frame.data(), frame.size()), 0);

    // Read the echo response
    std::string echo = read_server_text_frame(fd, 2000);
    ASSERT_EQ(echo, std::string("echo: hello"));
    ASSERT_EQ(echoCount.load(), 1);

    // Close the client connection
    ::close(fd);

    // Give the server time to clean up (readLoop returns, removeClient)
    std::this_thread::sleep_for(500ms);

    // connectionCount should drop back to 0
    ASSERT_EQ(ws.connectionCount("/echo"), size_t{0});

    ws.stop();
}

TEST(broadcast_reaches_clients) {
    // After the upgrade handoff, broadcast() should reach all
    // connected clients on the same path.
    OmniNet::HTTP::Server::Config cfg;
    cfg.port = 19103;
    cfg.workerCount = 2;  // need 2 workers for 2 concurrent clients
    WebSocket::Server ws(cfg);

    ws.on("/chat", [](Connection& conn) {
        conn.onText([](std::string) {
            // ignore
        });
        conn.readLoop();
    });

    JoinableThread t([&]() { ws.start(); });
    std::this_thread::sleep_for(200ms);

    // Connect 2 clients
    int fd1 = tcp_connect(19103);
    int fd2 = tcp_connect(19103);
    ASSERT_TRUE(fd1 >= 0 && fd2 >= 0);

    auto do_handshake = [](int fd) {
        const std::string req =
            "GET /chat HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        if (send_all(fd, req.data(), req.size()) != 0) return false;
        std::string resp;
        if (recv_until(fd, resp, "\r\n\r\n", 2000) != 0) return false;
        return resp.find("101 Switching Protocols") != std::string::npos;
    };
    ASSERT_TRUE(do_handshake(fd1));
    ASSERT_TRUE(do_handshake(fd2));

    // Wait for both to register
    for (int i = 0; i < 30 && ws.connectionCount("/chat") < 2; ++i) {
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_EQ(ws.connectionCount("/chat"), size_t{2});

    // Broadcast a message
    ws.broadcast("hello-all");

    // Both clients should receive it
    std::string m1 = read_server_text_frame(fd1, 2000);
    std::string m2 = read_server_text_frame(fd2, 2000);
    ASSERT_EQ(m1, std::string("hello-all"));
    ASSERT_EQ(m2, std::string("hello-all"));

    // Close one client
    ::close(fd1);
    for (int i = 0; i < 30 && ws.connectionCount("/chat") > 1; ++i) {
        std::this_thread::sleep_for(100ms);
    }
    // Either 0 or 1 depending on whether the other thread also cleaned up
    ASSERT_TRUE(ws.connectionCount("/chat") <= size_t{1});

    ::close(fd2);
    std::this_thread::sleep_for(500ms);
    ASSERT_EQ(ws.connectionCount("/chat"), size_t{0});

    ws.stop();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "WebSocket end-to-end tests (v0.7.0)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
