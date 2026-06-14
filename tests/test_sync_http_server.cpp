// OmniNet - HTTP::Server Sync Tests (v0.6.0 Phase 10d)
// Tests for src/http/HttpServer.cpp - zero coverage before this file
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <utility>
#include <unistd.h>

#include "http/HttpServer.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace std::chrono_literals;

// RAII thread wrapper: safe double-call guard for join()
struct JoinableThread {
    std::thread t;
    template<typename F, typename... Args>
    explicit JoinableThread(F&& f, Args&&... args)
        : t(std::forward<F>(f), std::forward<Args>(args)...) {}
    ~JoinableThread() {
        if (t.joinable()) {
            t.join();
        }
    }
};

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
        std::ostringstream oss; \
        oss << "ASSERT_EQ failed: " #a << " (" << (a) << ") != " #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

// ============================================================================
// Helpers
// ============================================================================

// Open a TCP connection to localhost:port, return fd or -1
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

// Read until '\n' or timeout (returns "" on timeout/error)
static std::string read_line(int fd, int timeout_ms = 3000) {
    std::string result;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    char ch;
    while (result.size() < 4096) {
        fd_set r = fds;
        int sel = ::select(fd + 1, &r, nullptr, nullptr, &tv);
        if (sel <= 0) break;
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0) break;
        if (ch == '\n') {
            if (!result.empty() && result.back() == '\r') result.pop_back();
            result.push_back('\n');
            break;
        }
        result.push_back(ch);
    }
    return result;
}

// Read exactly n bytes or until timeout
static std::string read_n(int fd, size_t n, int timeout_ms = 3000) {
    std::string result;
    result.reserve(n);
    size_t received = 0;
    while (received < n) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        fd_set r = fds;
        int sel = ::select(fd + 1, &r, nullptr, nullptr, &tv);
        if (sel <= 0) break;
        char buf[4096];
        ssize_t needed = static_cast<ssize_t>(std::min(n - received, sizeof(buf)));
        ssize_t nread = ::recv(fd, buf, needed, 0);
        if (nread <= 0) break;
        result.append(buf, static_cast<size_t>(nread));
        received += static_cast<size_t>(nread);
    }
    return result;
}

// Send raw HTTP request and return full response bytes
static std::string send_raw(int fd, const std::string& request) {
    size_t sent = 0;
    while (sent < request.size()) {
        ssize_t n = ::send(fd, request.data() + sent,
                          static_cast<size_t>(request.size() - sent), MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
    // Read response
    std::string response;
    char buf[4096];
    // Use a short timeout per read; close the fd when done
    for (int i = 0; i < 50; ++i) {  // up to 50 chunks
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv{1, 0};  // 1 second per chunk
        fd_set r = fds;
        int sel = ::select(fd + 1, &r, nullptr, nullptr, &tv);
        if (sel <= 0) break;
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, static_cast<size_t>(n));
    }
    return response;
}

// Parse HTTP/1.x status line: "HTTP/1.1 200 OK" -> 200
static int parse_status(const std::string& response) {
    std::istringstream iss(response);
    std::string http_ver;
    int status_code;
    iss >> http_ver >> status_code;
    (void)http_ver;
    return status_code;
}

// Extract header value from response
static std::string get_header(const std::string& response, const std::string& name) {
    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            // trim leading spaces
            while (!val.empty() && val[0] == ' ') val.erase(val.begin());
            if (key == name) return val;
        }
    }
    return "";
}

// ============================================================================
// Tests
// ============================================================================

TEST(server_start_stop) {
    // Verify server starts and stops cleanly
    Server::Config cfg;
    cfg.port = 18080;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/test", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("ok");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);
    ASSERT_TRUE(server.isRunning());

    server.stop();
    ASSERT_TRUE(!server.isRunning());
}

TEST(server_basic_get) {
    Server::Config cfg;
    cfg.port = 18081;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/api/hello", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "hello"})");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18081);
    ASSERT_TRUE(fd >= 0);

    std::string req = "GET /api/hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_TRUE(!response.empty());
    ASSERT_EQ(parse_status(response), 200);
    ASSERT_TRUE(response.find("application/json") != std::string::npos);
    ASSERT_TRUE(response.find(R"({"message": "hello"})") != std::string::npos);

    server.stop();
}

TEST(server_post_request_body) {
    // POST /api/echo should echo back the body
    Server::Config cfg;
    cfg.port = 18082;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().post("/api/echo", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/plain");
        resp.setBody(req.bodyAsString());
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18082);
    ASSERT_TRUE(fd >= 0);

    std::string body = "name=Alice&age=30";
    std::string req = "POST /api/echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;

    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_TRUE(!response.empty());
    ASSERT_EQ(parse_status(response), 200);
    ASSERT_TRUE(response.find(body) != std::string::npos);

    server.stop();
}

TEST(server_not_found) {
    // Unmatched routes should return 404
    Server::Config cfg;
    cfg.port = 18083;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/api/only", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("exists");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18083);
    ASSERT_TRUE(fd >= 0);

    std::string req = "GET /api/missing HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_TRUE(!response.empty());
    ASSERT_EQ(parse_status(response), 404);

    server.stop();
}

TEST(server_method_not_allowed) {
    // GET-only route should return 405 for POST
    Server::Config cfg;
    cfg.port = 18084;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/resource", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("get");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18084);
    ASSERT_TRUE(fd >= 0);

    std::string req = "POST /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_TRUE(!response.empty());
    int status = parse_status(response);
    ASSERT_TRUE(status == 404 || status == 405);  // 404 or 405 acceptable

    server.stop();
}

TEST(server_multiple_routes) {
    // Multiple registered routes work correctly
    Server::Config cfg;
    cfg.port = 18085;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/a", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("a");
    });
    server.router().get("/b", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("b");
    });
    server.router().get("/c", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("c");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    const char* paths[] = {"/a", "/b", "/c"};
    for (auto path : paths) {
        int fd = tcp_connect(18085);
        ASSERT_TRUE(fd >= 0);
        std::string req = std::string("GET ") + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        std::string response = send_raw(fd, req);
        ::close(fd);
        ASSERT_EQ(parse_status(response), 200);
        ASSERT_TRUE(response.find(path + 1) != std::string::npos);  // "a", "b", or "c"
    }

    server.stop();
}

TEST(server_keep_alive) {
    // HTTP/1.1 keep-alive: second request on same connection
    Server::Config cfg;
    cfg.port = 18086;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/first", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("first");
    });
    server.router().get("/second", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("second");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18086);
    ASSERT_TRUE(fd >= 0);

    // First request (no explicit Connection: close, so keep-alive by default)
    std::string req1 = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string resp1 = send_raw(fd, req1);
    ASSERT_TRUE(!resp1.empty());
    ASSERT_EQ(parse_status(resp1), 200);

    // Second request on same connection
    std::string req2 = "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string resp2 = send_raw(fd, req2);
    ::close(fd);

    ASSERT_TRUE(!resp2.empty());
    ASSERT_EQ(parse_status(resp2), 200);
    ASSERT_TRUE(resp2.find("second") != std::string::npos);

    server.stop();
}

TEST(server_content_length_set) {
    // v0.6.0 fix: Response::build() now auto-sets Content-Length.
    // Verify the server sends it.
    Server::Config cfg;
    cfg.port = 18087;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/json", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"key": "value"})");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    int fd = tcp_connect(18087);
    ASSERT_TRUE(fd >= 0);

    std::string req = "GET /json HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_TRUE(!response.empty());
    ASSERT_EQ(parse_status(response), 200);
    std::string cl = get_header(response, "Content-Length");
    ASSERT_TRUE(!cl.empty());  // Content-Length must be present
    // Body is 17 bytes: {"key": "value"}
    ASSERT_EQ(std::stoi(cl), 17);

    server.stop();
}

TEST(server_multiple_workers) {
    // Server with 4 workers should handle concurrent requests
    Server::Config cfg;
    cfg.port = 18088;
    cfg.workerCount = 4;
    Server server(cfg);
    server.router().get("/concurrent", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("concurrent");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(300ms);

    // Launch 8 concurrent connections
    std::vector<std::thread> clients;
    std::atomic<int> success_count{0};
    for (int i = 0; i < 8; ++i) {
        clients.emplace_back([&]() {
            int fd = tcp_connect(18088);
            if (fd >= 0) {
                std::string req = "GET /concurrent HTTP/1.1\r\nHost: localhost\r\n\r\n";
                std::string response = send_raw(fd, req);
                ::close(fd);
                if (parse_status(response) == 200) success_count++;
            }
        });
    }
    for (auto& c : clients) c.join();

    ASSERT_EQ(success_count.load(), 8);

    server.stop();
}

// ============================================================================
// v0.7.0 Phase 11a: boundPort() tests
// ============================================================================

TEST(bound_port_zero_before_start) {
    // boundPort() must return 0 before start() is called.
    Server::Config cfg;
    cfg.port = 18100;
    Server server(cfg);
    ASSERT_EQ(server.boundPort(), 0);
}

TEST(bound_port_matches_configured) {
    // When the server binds to an explicit port, boundPort() must
    // return that same port.
    Server::Config cfg;
    cfg.port = 18101;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    ASSERT_TRUE(server.isRunning());
    ASSERT_EQ(server.boundPort(), 18101);

    server.stop();
}

TEST(bound_port_ephemeral) {
    // When Config::port = 0, the OS assigns an ephemeral port.
    // boundPort() must return that real, non-zero port.
    Server::Config cfg;
    cfg.port = 0;             // OS-assigned
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    uint16_t actual = server.boundPort();
    ASSERT_TRUE(server.isRunning());
    ASSERT_TRUE(actual != 0);          // kernel picked something
    ASSERT_TRUE(actual >= 1024);       // ephemeral range

    server.stop();
}

TEST(bound_port_listenable) {
    // boundPort() must be a port that a real client can connect to.
    Server::Config cfg;
    cfg.port = 0;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/probe", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setBody("ok");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    uint16_t actual = server.boundPort();
    ASSERT_TRUE(actual != 0);

    // Connect to the reported port
    int fd = tcp_connect(actual);
    ASSERT_TRUE(fd >= 0);  // if this fails, boundPort() lied

    std::string req = "GET /probe HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string response = send_raw(fd, req);
    ::close(fd);

    ASSERT_EQ(parse_status(response), 200);
    ASSERT_TRUE(response.find("ok") != std::string::npos);

    server.stop();
}


// ============================================================================
// v0.7.0 Phase 11c: onReady() tests
// ============================================================================

#include <atomic>

TEST(on_ready_fires) {
    // Callback should fire exactly once after start().
    Server::Config cfg;
    cfg.port = 18200;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    std::atomic<int> callCount{0};
    uint16_t receivedPort = 0;
    server.setOnReady([&](uint16_t port) {
        ++callCount;
        receivedPort = port;
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    ASSERT_EQ(callCount.load(), 1);
    ASSERT_EQ(receivedPort, 18200);

    server.stop();
}

TEST(on_ready_receives_bound_port) {
    // onReady's argument must match boundPort().
    Server::Config cfg;
    cfg.port = 0;  // ephemeral
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    uint16_t cbPort = 0;
    server.setOnReady([&](uint16_t port) { cbPort = port; });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);

    ASSERT_TRUE(cbPort != 0);
    ASSERT_EQ(cbPort, server.boundPort());

    server.stop();
}

TEST(on_ready_not_set) {
    // No callback = no crash.
    Server::Config cfg;
    cfg.port = 18201;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    // No setOnReady call
    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);
    ASSERT_TRUE(server.isRunning());
    server.stop();
}

TEST(on_ready_throws) {
    // A callback that throws is caught; server keeps running.
    Server::Config cfg;
    cfg.port = 18202;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    server.setOnReady([](uint16_t) {
        throw std::runtime_error("user code blew up");
    });

    JoinableThread t([&]() { server.start(); });
    std::this_thread::sleep_for(200ms);
    ASSERT_TRUE(server.isRunning());  // didn't die
    server.stop();
}

TEST(on_ready_async_fires) {
    // startAsync() eventually fires the callback.
    Server::Config cfg;
    cfg.port = 18203;
    cfg.workerCount = 1;
    Server server(cfg);
    server.router().get("/x", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::OK);
    });

    std::atomic<bool> called{false};
    server.setOnReady([&](uint16_t) { called = true; });

    server.startAsync();

    // Poll for the callback (up to 2 seconds).
    for (int i = 0; i < 20 && !called.load(); ++i) {
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_TRUE(called.load());

    server.stop();
}


// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "HTTP::Server Sync Tests (v0.6.0)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Static initializers run all TEST() registrations
    // (each TEST() macro registers itself via a static variable)

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
