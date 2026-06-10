// OmniNet - AsyncServer Unit Tests
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

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
