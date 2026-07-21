// OmniNet - StreamingResponse tests
//
// End-to-end tests for the request-scoped streaming response writer
// introduced in v0.9.0 (prompt.txt A2A streaming).

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "net/Socket.h"
#include "http/HttpServer.h"
#include "http/StreamingResponse.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace std::chrono_literals;

namespace {

int g_passed = 0;
int g_failed = 0;

#define EXPECT(cond)                                                        \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "  FAIL: " << #cond                                \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";   \
            ++g_failed;                                                     \
            return;                                                         \
        }                                                                   \
    } while (0)

#define EXPECT_EQ(a, b)                                                     \
    do {                                                                    \
        auto va = (a); auto vb = (b);                                       \
        if (!(va == vb)) {                                                  \
            std::cerr << "  FAIL: " << #a << " (" << va << ") != "         \
                      << #b << " (" << vb << ") at " << __FILE__ << ":"   \
                      << __LINE__ << "\n";                                  \
            ++g_failed;                                                     \
            return;                                                         \
        }                                                                   \
    } while (0)

#define PASS(name)                                                          \
    do { std::cout << "  PASS: " << name << "\n"; ++g_passed; } while (0)

// ============================================================================
// Helpers
// ============================================================================

bool waitForConnected(const std::shared_ptr<StreamingResponse>& stream,
                      std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (stream->client_connected()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

uint16_t findFreePort() {
    ServerSocket probe;
    probe.bindAndListen(0, "127.0.0.1", 1);
    uint16_t port = probe.localPort();
    probe.close();
    return port;
}

struct TestServer {
    std::unique_ptr<HTTP::Server> server;
    std::thread thread;
    uint16_t port = 0;

    void start(HTTP::Handler handler) {
        port = findFreePort();

        HTTP::Server::Config cfg;
        cfg.address = "127.0.0.1";
        cfg.port = port;
        cfg.workerCount = 2;
        cfg.requestTimeout = 30s;
        cfg.keepAliveTimeout = 1s;

        server = std::make_unique<HTTP::Server>(cfg);
        server->post("/stream", std::move(handler));

        thread = std::thread([this] { server->start(); });

        for (int i = 0; i < 200; ++i) {
            Socket s;
            try {
                s.connect("127.0.0.1", port);
                if (s.isOpen()) {
                    return;
                }
            } catch (...) {
            }
            std::this_thread::sleep_for(10ms);
        }
    }

    void stop() {
        if (server) {
            server->stop();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }
};

class StreamClient {
public:
    StreamClient(const std::string& host, uint16_t port)
        : m_host(host), m_port(port) {
        m_sock.connect(host, port);
    }

    ~StreamClient() { close(); }

    void sendRequest(const std::string& path) {
        std::ostringstream req;
        req << "POST " << path << " HTTP/1.1\r\n"
            << "Host: " << m_host << ":" << m_port << "\r\n"
            << "\r\n";
        const auto s = req.str();
        m_sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(s.data()), s.size()));
    }

    std::string readHeaders() {
        std::string buf;
        char c = 0;
        m_sock.setRecvTimeout(2s);
        while (true) {
            auto n = m_sock.recv(std::span<uint8_t>(
                reinterpret_cast<uint8_t*>(&c), 1));
            if (n == 0) {
                throw std::runtime_error("readHeaders: peer closed");
            }
            buf.push_back(c);
            if (buf.size() >= 4 &&
                buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0) {
                return buf;
            }
        }
    }

    // Read one chunked frame and return the payload. Returns an empty
    // string for the terminating 0-length chunk.
    std::string readChunk() {
        m_sock.setRecvTimeout(2s);

        std::string sizeLine;
        char c = 0;
        while (true) {
            auto n = m_sock.recv(std::span<uint8_t>(
                reinterpret_cast<uint8_t*>(&c), 1));
            if (n == 0) {
                throw std::runtime_error("readChunk: peer closed (size)");
            }
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                break;
            }
            sizeLine.push_back(c);
        }

        if (sizeLine.empty()) {
            throw std::runtime_error("readChunk: empty size line");
        }

        size_t size = 0;
        try {
            size = std::stoull(sizeLine, nullptr, 16);
        } catch (...) {
            throw std::runtime_error("readChunk: bad hex size: " + sizeLine);
        }

        if (size == 0) {
            (void)m_sock.recvExactly(2);  // trailing \r\n
            return std::string{};
        }

        auto payloadBytes = m_sock.recvExactly(size);
        std::string payload(payloadBytes.begin(), payloadBytes.end());
        (void)m_sock.recvExactly(2);  // trailing \r\n
        return payload;
    }

    bool isOpen() const { return m_sock.isOpen(); }

    void close() {
        if (m_sock.isOpen()) {
            try {
                m_sock.close();
            } catch (...) {
            }
        }
    }

private:
    std::string m_host;
    uint16_t m_port = 0;
    Socket m_sock;
};

// ============================================================================
// Tests
// ============================================================================

void test_basic_streaming() {
    std::cout << "\n[streaming_basic]\n";

    TestServer srv;
    srv.start([](const Request&, Response& resp) {
        auto stream = resp.startStreaming("text/event-stream");
        std::thread([stream] {
            if (!waitForConnected(stream, 2s)) {
                stream->close();
                return;
            }
            stream->writeSseEvent("statusUpdate",
                                  R"({"state":"working"})");
            std::this_thread::sleep_for(50ms);
            stream->writeSseEvent("artifactUpdate",
                                  R"({"artifact":"x"})");
            stream->close();
        }).detach();
    });

    StreamClient client("127.0.0.1", srv.port);
    client.sendRequest("/stream");

    auto headers = client.readHeaders();
    EXPECT(headers.find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT(headers.find("Transfer-Encoding: chunked") != std::string::npos);
    EXPECT(headers.find("Content-Type: text/event-stream") !=
           std::string::npos);
    EXPECT(headers.find("Cache-Control: no-cache") != std::string::npos);

    auto chunk1 = client.readChunk();
    EXPECT(chunk1.find("event: statusUpdate") != std::string::npos);
    EXPECT(chunk1.find(R"(data: {"state":"working"})") != std::string::npos);

    auto chunk2 = client.readChunk();
    EXPECT(chunk2.find("event: artifactUpdate") != std::string::npos);
    EXPECT(chunk2.find(R"(data: {"artifact":"x"})") != std::string::npos);

    auto chunk3 = client.readChunk();
    EXPECT_EQ(chunk3.size(), 0u);  // chunked terminator

    srv.stop();
    PASS("streaming_basic");
}

void test_disconnect_callback() {
    std::cout << "\n[streaming_disconnect_callback]\n";

    std::atomic<bool> disconnected{false};
    TestServer srv;
    srv.start([&disconnected](const Request&, Response& resp) {
        auto stream = resp.startStreaming("text/event-stream");
        stream->on_disconnect([&disconnected] { disconnected.store(true); });

        std::thread([stream] {
            if (!waitForConnected(stream, 2s)) {
                stream->close();
                return;
            }
            stream->writeSseEvent("statusUpdate", "{}");
            // Wait until the client goes away.
            for (int i = 0; i < 100 && stream->client_connected(); ++i) {
                std::this_thread::sleep_for(50ms);
            }
            stream->close();
        }).detach();
    });

    StreamClient client("127.0.0.1", srv.port);
    client.sendRequest("/stream");
    (void)client.readHeaders();
    auto chunk = client.readChunk();
    EXPECT(chunk.find("event: statusUpdate") != std::string::npos);

    client.close();

    // Wait for the server-side disconnect callback.
    for (int i = 0; i < 100 && !disconnected.load(); ++i) {
        std::this_thread::sleep_for(50ms);
    }
    EXPECT(disconnected.load());

    srv.stop();
    PASS("streaming_disconnect_callback");
}

void test_server_stop_closes_stream() {
    std::cout << "\n[streaming_server_stop_closes_stream]\n";

    std::atomic<bool> workerDone{false};
    std::atomic<bool> sawDisconnected{false};
    TestServer srv;
    srv.start([&workerDone, &sawDisconnected](const Request&, Response& resp) {
        auto stream = resp.startStreaming("text/event-stream");
        std::thread([stream, &workerDone, &sawDisconnected] {
            if (!waitForConnected(stream, 2s)) {
                sawDisconnected.store(!stream->client_connected());
                workerDone.store(true);
                stream->close();
                return;
            }
            for (int i = 0; i < 200 && stream->client_connected(); ++i) {
                stream->writeSseEvent("ping", "{}");
                std::this_thread::sleep_for(50ms);
            }
            sawDisconnected.store(!stream->client_connected());
            workerDone.store(true);
            stream->close();
        }).detach();
    });

    StreamClient client("127.0.0.1", srv.port);
    client.sendRequest("/stream");
    (void)client.readHeaders();
    (void)client.readChunk();  // first ping

    // Stopping the server must close the streaming socket and unblock
    // the worker thread.
    srv.stop();

    // Give the worker a moment to observe the disconnect and set the flag.
    for (int i = 0; i < 100 && !workerDone.load(); ++i) {
        std::this_thread::sleep_for(50ms);
    }

    EXPECT(workerDone.load());
    EXPECT(sawDisconnected.load());
    PASS("streaming_server_stop_closes_stream");
}

void test_write_raw_binary() {
    std::cout << "\n[streaming_write_raw_binary]\n";

    TestServer srv;
    srv.start([](const Request&, Response& resp) {
        auto stream = resp.startStreaming("application/octet-stream");
        std::thread([stream] {
            if (!waitForConnected(stream, 2s)) {
                stream->close();
                return;
            }
            const std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
            stream->write(std::span<const uint8_t>(data));
            stream->close();
        }).detach();
    });

    StreamClient client("127.0.0.1", srv.port);
    client.sendRequest("/stream");

    auto headers = client.readHeaders();
    EXPECT(headers.find("Content-Type: application/octet-stream") !=
           std::string::npos);

    auto chunk = client.readChunk();
    EXPECT_EQ(chunk.size(), 4u);
    EXPECT_EQ(static_cast<uint8_t>(chunk[0]), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(chunk[1]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(chunk[2]), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(chunk[3]), 0x03);

    auto term = client.readChunk();
    EXPECT_EQ(term.size(), 0u);

    srv.stop();
    PASS("streaming_write_raw_binary");
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main() {
    test_basic_streaming();
    test_disconnect_callback();
    test_server_stop_closes_stream();
    test_write_raw_binary();

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << g_passed << "\n";
    std::cout << "Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
