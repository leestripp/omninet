// OmniNet - AsyncServer Unit Tests
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

#include "async/Task.h"
#include "async/Executor.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "io/IoContext.h"
#include "io/EpollContext.h"
#include "server/AsyncServer.h"
#include "thread/ThreadPool.h"

using namespace OmniNet;
using namespace OmniNet::Server;
using namespace OmniNet::Async;
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
        throw std::runtime_error("ASSERT_EQ failed"); \
    } \
} while(0)

// Helper to create test environment
struct TestEnv {
    Thread::ThreadPool::Config poolCfg;
    std::unique_ptr<Thread::ThreadPool> pool;
    std::unique_ptr<ThreadPoolExecutor> exec;
    std::unique_ptr<IO::EpollIoContext> ctx;

    TestEnv() {
        poolCfg.workerCount = 2;
        pool = std::make_unique<Thread::ThreadPool>(poolCfg);
        exec = std::make_unique<ThreadPoolExecutor>(*pool);
        ctx = std::make_unique<IO::EpollIoContext>(*exec);
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST(construct_with_default_config) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19001;  // Use non-standard port
    AsyncServer server(*env.ctx, cfg);
    ASSERT_TRUE(!server.running());
    ASSERT_EQ(server.stats().connectionsTotal.load(), (uint64_t)0);
}

TEST(construct_with_default_constructor) {
    TestEnv env;
    AsyncServer server(*env.ctx);
    ASSERT_TRUE(!server.running());
}

TEST(get_router) {
    TestEnv env;
    AsyncServer server(*env.ctx);
    HTTP::Router& r = server.router();
    (void)r;  // Just verify we can access it
}

TEST(register_async_handler) {
    TestEnv env;
    AsyncServer server(*env.ctx);

    // This should not throw
    server.get("/test", [](const HTTP::Request& req, Async::Executor& exec)
                       -> Async::Task<HTTP::Response> {
        (void)req;
        (void)exec;
        HTTP::Response resp;
        resp.setBody("test");
        co_return resp;
    });
}

TEST(stats_initial) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19002;
    AsyncServer server(*env.ctx, cfg);
    const auto& stats = server.stats();
    ASSERT_EQ(stats.requestsTotal.load(), (uint64_t)0);
    ASSERT_EQ(stats.connectionsTotal.load(), (uint64_t)0);
}

TEST(lifecycle_run_stop) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19003;
    AsyncServer server(*env.ctx, cfg);

    server.run_async();
    std::this_thread::sleep_for(50ms);  // Give time to start
    ASSERT_TRUE(server.running());

    server.stop();
    ASSERT_TRUE(!server.running());
}

// Helper: send raw HTTP request and read response
static std::string http_get(uint16_t port, const std::string& path) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return {};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return {};
    }

    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Connection: close\r\n"
                          "\r\n";
    ::send(fd, request.data(), request.size(), 0);

    std::string response;
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);
    return response;
}

TEST(async_handler_dispatch_basic) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19010;
    AsyncServer server(*env.ctx, cfg);

    server.get("/hello", [](const HTTP::Request&, Async::Executor&)
                       -> Async::Task<HTTP::Response> {
        HTTP::Response resp;
        resp.setStatus(HTTP::StatusCode::OK);
        resp.setContentType("text/plain");
        resp.setBody("Hello, async world!");
        co_return resp;
    });

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto response = http_get(19010, "/hello");
    server.stop();

    ASSERT_TRUE(!response.empty());
    ASSERT_TRUE(response.find("HTTP/1.1 200") != std::string::npos);
    ASSERT_TRUE(response.find("Hello, async world!") != std::string::npos);
}

TEST(async_handler_not_found) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19011;
    AsyncServer server(*env.ctx, cfg);

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto response = http_get(19011, "/nope");
    server.stop();

    ASSERT_TRUE(!response.empty());
    ASSERT_TRUE(response.find("404") != std::string::npos);
}

TEST(async_handler_path_param) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19012;
    AsyncServer server(*env.ctx, cfg);

    server.get("/users/:id", [](const HTTP::Request& req, Async::Executor&)
                          -> Async::Task<HTTP::Response> {
        HTTP::Response resp;
        resp.setContentType("text/plain");
        resp.setBody("user:" + std::string(req.path()));
        co_return resp;
    });

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto response = http_get(19012, "/users/42");
    server.stop();

    ASSERT_TRUE(!response.empty());
    ASSERT_TRUE(response.find("200") != std::string::npos);
    ASSERT_TRUE(response.find("user:/users/42") != std::string::npos);
}

TEST(async_handler_wildcard) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19013;
    AsyncServer server(*env.ctx, cfg);

    server.get("/static/*", [](const HTTP::Request& req, Async::Executor&)
                          -> Async::Task<HTTP::Response> {
        HTTP::Response resp;
        resp.setBody("static:" + std::string(req.path()));
        co_return resp;
    });

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto response = http_get(19013, "/static/foo/bar.txt");
    server.stop();

    ASSERT_TRUE(!response.empty());
    ASSERT_TRUE(response.find("200") != std::string::npos);
    ASSERT_TRUE(response.find("static:/static/foo/bar.txt") != std::string::npos);
}

TEST(async_handler_chain) {
    // Test that the AsyncServer correctly awaits a chained async coroutine
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19014;
    AsyncServer server(*env.ctx, cfg);

    // Handler that does co_await on another coroutine (which uses co_await internally)
    server.get("/chain", [](const HTTP::Request&, Async::Executor& exec)
                         -> Async::Task<HTTP::Response> {
        // Inner coroutine: an async task that uses co_await
        auto inner = [&exec]() -> Async::Task<std::string> {
            // co_await on an executor submit (returns future, we wrap as coroutine)
            auto fut = exec.submit([]() {
                std::this_thread::sleep_for(10ms);
                return std::string("inner-result");
            });
            // co_await equivalent: dispatch to executor and return
            // For a real test we'd use an async I/O awaiter. Here we use the
            // fact that co_return from inside a coroutine is supported.
            std::string v = fut.get();
            co_return v;
        };

        // Await the inner coroutine
        std::string result = co_await inner();

        HTTP::Response resp;
        resp.setBody("chain:" + result);
        co_return resp;
    });

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto response = http_get(19014, "/chain");
    server.stop();

    ASSERT_TRUE(!response.empty());
    ASSERT_TRUE(response.find("200") != std::string::npos);
    ASSERT_TRUE(response.find("chain:inner-result") != std::string::npos);
}

TEST(async_handler_stats) {
    TestEnv env;
    AsyncServer::Config cfg;
    cfg.port = 19015;
    AsyncServer server(*env.ctx, cfg);

    server.get("/x", [](const HTTP::Request&, Async::Executor&)
                    -> Async::Task<HTTP::Response> {
        HTTP::Response resp;
        resp.setBody("x");
        co_return resp;
    });

    server.run_async();
    std::this_thread::sleep_for(50ms);

    auto r1 = http_get(19015, "/x");
    auto r2 = http_get(19015, "/x");
    auto r3 = http_get(19015, "/nope");

    const auto& stats = server.stats();
    server.stop();

    ASSERT_TRUE(!r1.empty());
    ASSERT_TRUE(!r2.empty());
    ASSERT_TRUE(!r3.empty());
    ASSERT_TRUE(stats.connectionsTotal.load() >= 3);
    ASSERT_TRUE(stats.requestsTotal.load() >= 3);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "AsyncServer Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
