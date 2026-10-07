// OmniNet HTTP Connection Tests
// Tests for src/http/HttpConnection.cpp - especially chunked transfer-encoding
// (v0.6.0 fix: previously chunked body data was discarded)
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "http/HttpConnection.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "net/Socket.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace std::chrono_literals;

// ============================================================================
// Test Framework
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            run_test(#name, test_##name); \
        } \
    } g_registrar_##name; \
    static void test_##name()

void run_test(const char* name, void (*func)()) {
    std::cout << "  " << name << "... ";
    try {
        func();
        std::cout << "PASS" << std::endl;
        ++g_passed;
    } catch (const std::exception& e) {
        std::cout << "FAIL (" << e.what() << ")" << std::endl;
        ++g_failed;
    } catch (...) {
        std::cout << "FAIL (unknown exception)" << std::endl;
        ++g_failed;
    }
}

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("ASSERT_TRUE failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("ASSERT_EQ failed: " #a " != " #b); \
    } \
} while(0)

// ============================================================================
// Helpers
// ============================================================================

// Send raw bytes to a socket (until closed or all sent)
static void send_all(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
}

// Open a TCP loopback connection to a local server socket
// Returns the connected fd; writes the listening port to *out_port
static int tcp_connect_to_localhost(uint16_t port) {
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

// Find a free TCP port by binding to port 0
static int bind_listening_socket(uint16_t& out_port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // any free port

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }

    if (::listen(fd, 16) < 0) {
        ::close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        ::close(fd);
        return -1;
    }
    out_port = ntohs(addr.sin_port);
    return fd;
}

// Captured request data
struct CapturedRequest {
    std::string method;
    std::string path;
    std::string body;
    std::atomic<bool> captured{false};
    std::atomic<int> count{0};
};

// Run a connection handler. Returns when handler is called, then signals done.
static void run_test_connection(std::unique_ptr<Connection> conn,
                                CapturedRequest& cap,
                                std::atomic<bool>& done) {
    conn->setHandler([&](const Request& req, Response& resp) {
        cap.method = std::string(req.methodString());
        cap.path = std::string(req.path());
        cap.body = std::string(req.bodyAsString());
        cap.captured.store(true);
        cap.count.fetch_add(1);
        resp.setStatus(StatusCode::OK);
        resp.setBody("OK");
    });
    // Don't call processRequests (which sends a response and may hang on
    // TCP). Just call readRequest-like behavior via the handler.
    // Actually, we need processRequests to invoke the handler.
    // The test just needs the handler to run - it doesn't need the
    // response to be sent. processRequests will send and may hang,
    // so we set a short recv timeout on the socket.
    conn->setRequestTimeout(std::chrono::seconds(2));
    conn->setKeepAliveTimeout(std::chrono::seconds(0));
    conn->processRequests();
    done.store(true);
}

// TCP-based test: spin up a server socket, accept a connection, run
// Connection on it, send a request, verify body.
static void run_tcp_test(const std::string& request,
                          CapturedRequest& cap,
                          size_t maxRequestSize = 0) {
    uint16_t port = 0;
    int listen_fd = bind_listening_socket(port);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to bind listening socket");
    }

    std::atomic<bool> accepted{false};
    std::atomic<bool> done{false};
    int client_fd = -1;

    // Accept thread
    std::thread acceptor([&]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr),
                            &client_len);
        if (cfd < 0) return;
        client_fd = cfd;
        accepted.store(true);

        // Set short recv timeout
        timeval tv{1, 0};  // 1s
        ::setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        auto sock = std::make_unique<Socket>(cfd);
        auto conn = std::make_unique<Connection>(std::move(sock));
        // Bug #7: without this the body cap is the hardcoded 10MB floor,
        // and a >10MB upload is dropped even when the server config allows it.
        if (maxRequestSize > 0) {
            conn->setMaxRequestSize(maxRequestSize);
        }
        run_test_connection(std::move(conn), cap, done);
    });

    // Client thread: connect, send, then close
    std::thread client([&]() {
        // Wait for server to be ready
        int cfd = tcp_connect_to_localhost(port);
        if (cfd < 0) return;
        send_all(cfd, request);
        // Keep the connection open briefly so the server can read
        std::this_thread::sleep_for(100ms);
        ::shutdown(cfd, SHUT_WR);
        std::this_thread::sleep_for(200ms);
        ::close(cfd);
    });

    // Wait up to 3s for the request to be captured
    auto start = std::chrono::steady_clock::now();
    while (!cap.captured.load() &&
           std::chrono::steady_clock::now() - start < 3s) {
        std::this_thread::sleep_for(10ms);
    }

    client.join();
    acceptor.join();
    ::close(listen_fd);
}

// ============================================================================
// Tests
// ============================================================================

TEST(chunked_single_chunk) {
    // The bug: chunked body data was discarded.
    // Single chunk: "5\r\nhello\r\n0\r\n\r\n" -> body should be "hello"
    const std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n"
        "0\r\n"
        "\r\n";

    CapturedRequest cap;
    run_tcp_test(request, cap);

    ASSERT_TRUE(cap.captured.load());
    ASSERT_EQ(cap.method, std::string("POST"));
    ASSERT_EQ(cap.path, std::string("/upload"));
    ASSERT_EQ(cap.body, std::string("hello"));
}

TEST(chunked_multiple_chunks) {
    // Three chunks: "hel" + "lo" + " world" -> body should be "hello world"
    const std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "3\r\n"
        "hel\r\n"
        "2\r\n"
        "lo\r\n"
        "6\r\n"
        " world\r\n"
        "0\r\n"
        "\r\n";

    CapturedRequest cap;
    run_tcp_test(request, cap);

    ASSERT_TRUE(cap.captured.load());
    ASSERT_EQ(cap.body, std::string("hello world"));
}

TEST(content_length_still_works) {
    // Regression test: ensure the fix to chunked doesn't break Content-Length
    const std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";

    CapturedRequest cap;
    run_tcp_test(request, cap);

    ASSERT_TRUE(cap.captured.load());
    ASSERT_EQ(cap.body, std::string("hello"));
}

TEST(get_request_no_body) {
    // GET requests have no body - ensure basic GET still works
    const std::string request =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    CapturedRequest cap;
    run_tcp_test(request, cap);

    ASSERT_TRUE(cap.captured.load());
    ASSERT_EQ(cap.method, std::string("GET"));
    ASSERT_EQ(cap.path, std::string("/hello"));
    ASSERT_EQ(cap.body, std::string(""));
}

TEST(invalid_chunk_size_returns_error) {
    // Non-hex chunk size should cause the connection to fail
    const std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "ZZZZ\r\n"  // Invalid hex
        "hello\r\n"
        "0\r\n"
        "\r\n";

    CapturedRequest cap;
    run_tcp_test(request, cap);

    // Handler should NOT have been called - the connection failed before parsing
    ASSERT_TRUE(!cap.captured.load());
}

// ============================================================================
// Main
// ============================================================================

TEST(large_body_accepted_within_raised_limit) {
    // v1.0.4 fix (bug #7, ChimpFlow report #1): a 12MB body must be
    // accepted when the server raises maxRequestSize. The pre-fix code
    // enforced a hardcoded 10MB SAFETY_BODY_LIMIT and dropped the request
    // mid-read regardless of the configured limit.
    const size_t bodySize = 12 * 1024 * 1024;  // 12MB > old 10MB cap
    std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: " + std::to_string(bodySize) + "\r\n"
        "\r\n";
    request.append(bodySize, 'x');

    CapturedRequest cap;
    run_tcp_test(request, cap, /*maxRequestSize=*/ 32 * 1024 * 1024);

    ASSERT_TRUE(cap.captured.load());
    ASSERT_EQ(cap.method, std::string("POST"));
    ASSERT_EQ(cap.path, std::string("/upload"));
    ASSERT_EQ(cap.body.size(), bodySize);
}

TEST(body_over_default_limit_still_rejected) {
    // The configured limit must still be enforced: with the default 1MB,
    // a 2MB body is rejected (handler never invoked).
    const size_t bodySize = 2 * 1024 * 1024;
    std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(bodySize) + "\r\n"
        "\r\n";
    request.append(bodySize, 'x');

    CapturedRequest cap;
    run_tcp_test(request, cap);  // default 1MB limit

    ASSERT_TRUE(!cap.captured.load());
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "HttpConnection Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
