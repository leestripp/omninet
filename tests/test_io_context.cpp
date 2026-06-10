// OmniNet - I/O Context Unit Tests
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "io/IoContext.h"
#include "io/EpollContext.h"
#include "io/AsyncSocket.h"
#include "async/Task.h"
#include "async/Executor.h"
#include "thread/ThreadPool.h"
#include "net/Socket.h"

using namespace OmniNet;
using namespace OmniNet::IO;
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

// ============================================================================
// Tests
// ============================================================================

TEST(create_epoll_context) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);
    ASSERT_TRUE(ctx.epoll_fd() >= 0);
    ASSERT_TRUE(!ctx.running());
}

TEST(run_once_empty) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    auto elapsed = ctx.run_once(10ms);
    // No events, so should return close to the timeout
    (void)elapsed;
}

TEST(post_executes_callback) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    std::atomic<bool> executed{false};

    // Use a std::jthread to drive the event loop briefly
    std::jthread runner([&ctx](std::stop_token st) {
        // Run for a short time
        auto deadline = std::chrono::steady_clock::now() + 100ms;
        while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline) {
            ctx.run_once(10ms);
        }
    });

    // Give the runner time to start
    std::this_thread::sleep_for(10ms);

    // Post a task
    ctx.post([&executed]() {
        executed.store(true);
    });

    // Wait for the task to be executed
    for (int i = 0; i < 50 && !executed.load(); ++i) {
        std::this_thread::sleep_for(10ms);
    }

    runner.request_stop();
    runner.join();

    ASSERT_TRUE(executed.load());
}

TEST(timer_fires) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    std::atomic<bool> fired{false};

    std::jthread runner([&ctx](std::stop_token st) {
        auto deadline = std::chrono::steady_clock::now() + 200ms;
        while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline) {
            ctx.run_once(10ms);
        }
    });

    std::this_thread::sleep_for(10ms);
    ctx.timer_after(50ms, [&fired]() {
        fired.store(true);
    });

    for (int i = 0; i < 30 && !fired.load(); ++i) {
        std::this_thread::sleep_for(10ms);
    }

    runner.request_stop();
    runner.join();
    ASSERT_TRUE(fired.load());
}

TEST(timer_cancel) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    std::atomic<bool> fired{false};
    uint64_t timerId = ctx.timer_after(50ms, [&fired]() {
        fired.store(true);
    });

    // Cancel immediately
    ctx.timer_cancel(timerId);

    std::jthread runner([&ctx](std::stop_token st) {
        auto deadline = std::chrono::steady_clock::now() + 200ms;
        while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline) {
            ctx.run_once(10ms);
        }
    });

    std::this_thread::sleep_for(100ms);

    runner.request_stop();
    runner.join();
    ASSERT_TRUE(!fired.load());
}

TEST(async_socket_basic_read) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    // Create a connected socket pair
    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto client = std::make_unique<Socket>(sv[0]);
    auto server = std::make_unique<Socket>(sv[1]);
    ASSERT_TRUE(client->isOpen());
    ASSERT_TRUE(server->isOpen());

    // Start runner thread
    std::atomic<bool> stop{false};
    std::jthread runner([&ctx, &stop](std::stop_token st) {
        while (!st.stop_requested() && !stop.load()) {
            ctx.run_once(10ms);
        }
    });

    AsyncSocket asyncSock(ctx, std::move(client));

    // Write some data
    const char* msg = "Hello, async!";
    ssize_t written = ::write(sv[1], msg, strlen(msg));
    ASSERT_EQ(written, (ssize_t)strlen(msg));

    // Read using coroutine
    auto reader = [&asyncSock]() -> Task<std::string> {
        std::array<uint8_t, 256> buf;
        size_t n = co_await asyncSock.read_some(buf);
        co_return std::string(reinterpret_cast<char*>(buf.data()), n);
    };

    auto task = reader();
    task.resume();
    auto result = std::move(task).result();

    stop.store(true);
    runner.request_stop();
    runner.join();

    server->close();
    ::close(sv[0]);
    ::close(sv[1]);

    ASSERT_EQ(result, std::string("Hello, async!"));
}

TEST(async_socket_echo) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock1 = std::make_unique<Socket>(sv[0]);
    auto sock2 = std::make_unique<Socket>(sv[1]);

    std::atomic<bool> stop{false};
    std::jthread runner([&ctx, &stop](std::stop_token st) {
        while (!st.stop_requested() && !stop.load()) {
            ctx.run_once(10ms);
        }
    });

    AsyncSocket async1(ctx, std::move(sock1));
    AsyncSocket async2(ctx, std::move(sock2));

    auto echo = [&async1, &async2]() -> Task<std::string> {
        const char* msg = "Echo!";
        std::span<const uint8_t> data(reinterpret_cast<const uint8_t*>(msg), 5);
        co_await async1.write_all(data);

        std::array<uint8_t, 256> buf;
        size_t n = co_await async2.read_some(buf);
        co_return std::string(reinterpret_cast<char*>(buf.data()), n);
    };

    auto task = echo();
    task.resume();
    auto result = std::move(task).result();

    stop.store(true);
    runner.request_stop();
    runner.join();

    async1.close();
    async2.close();
    ::close(sv[0]);
    ::close(sv[1]);

    ASSERT_EQ(result, std::string("Echo!"));
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "I/O Context Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
