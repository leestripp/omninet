// OmniNet - AsyncLogger Unit Tests
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "async/AsyncLogger.h"
#include "thread/ThreadPool.h"
#include "utils/Logger.h"

using namespace OmniNet;
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

// Helper to create a unique log file path
static std::string makeLogPath() {
    static int counter = 0;
    return "/tmp/test_async_log_" + std::to_string(getpid()) + "_" +
           std::to_string(++counter) + ".log";
}

// ============================================================================
// Tests
// ============================================================================

TEST(construct_default) {
    AsyncLogger logger;
    ASSERT_TRUE(logger.running());
    logger.shutdown();
}

TEST(construct_with_thread_pool) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    AsyncLogger logger(pool);
    ASSERT_TRUE(logger.running());
    logger.shutdown();
}

TEST(basic_log) {
    auto path = makeLogPath();
    std::remove(path.c_str());

    AsyncLogger logger;
    logger.setLogFile(path);
    logger.info("Hello, world!");
    logger.flush();

    // Verify file was written
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    ASSERT_TRUE(content.find("Hello, world!") != std::string::npos);

    logger.shutdown();
    std::remove(path.c_str());
}

TEST(non_blocking_log) {
    AsyncLogger logger;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        logger.info("Message " + std::to_string(i));
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    // 1000 logs should take < 100ms (non-blocking)
    ASSERT_TRUE(elapsed < 100ms);
    logger.flush();
    logger.shutdown();
}

TEST(concurrent_log) {
    AsyncLogger logger;
    constexpr int N_THREADS = 10;
    constexpr int N_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < N_THREADS; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < N_PER_THREAD; ++i) {
                logger.info("Thread " + std::to_string(t) + " msg " + std::to_string(i));
            }
        });
    }

    for (auto& t : threads) t.join();
    logger.flush();

    const auto& stats = logger.stats();
    ASSERT_EQ(stats.messagesQueued.load(), (uint64_t)(N_THREADS * N_PER_THREAD));
    ASSERT_EQ(stats.messagesWritten.load(), (uint64_t)(N_THREADS * N_PER_THREAD));
    ASSERT_EQ(stats.messagesDropped.load(), (uint64_t)0);
    logger.shutdown();
}

TEST(flush_drains_queue) {
    AsyncLogger logger;
    for (int i = 0; i < 100; ++i) {
        logger.info("Test");
    }
    const auto& s1 = logger.stats();
    ASSERT_EQ(s1.messagesQueued.load(), (uint64_t)100);

    logger.flush();
    const auto& s2 = logger.stats();
    ASSERT_EQ(s2.messagesWritten.load(), (uint64_t)100);
    logger.shutdown();
}

TEST(shutdown_drains) {
    auto path = makeLogPath();
    std::remove(path.c_str());

    {
        AsyncLogger logger;
        logger.setLogFile(path);
        for (int i = 0; i < 50; ++i) {
            logger.info("Drain test " + std::to_string(i));
        }
        // Destructor should call shutdown, which flushes
    }

    // Verify all messages were written
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("Drain test", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    ASSERT_EQ(count, (size_t)50);

    std::remove(path.c_str());
}

TEST(queue_full_drops) {
    AsyncLogger::Config cfg;
    cfg.maxQueueSize = 10;
    cfg.flushInterval = 1s;  // Slow flush to keep queue full
    AsyncLogger logger(cfg);

    // First fill the queue (it'll drain slowly)
    for (int i = 0; i < 100; ++i) {
        logger.info("msg " + std::to_string(i));
    }

    const auto& stats = logger.stats();
    // Some messages should have been dropped
    ASSERT_TRUE(stats.messagesDropped.load() > 0);

    logger.shutdown();
}

TEST(level_filtering) {
    auto path = makeLogPath();
    std::remove(path.c_str());

    AsyncLogger logger;
    logger.setLogFile(path);
    logger.setLevel(LogLevel::WARN);

    logger.debug("debug msg");      // filtered
    logger.info("info msg");         // filtered
    logger.warn("warn msg");         // kept
    logger.error("error msg");       // kept
    logger.flush();
    logger.shutdown();

    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    ASSERT_TRUE(content.find("debug msg") == std::string::npos);
    ASSERT_TRUE(content.find("info msg") == std::string::npos);
    ASSERT_TRUE(content.find("warn msg") != std::string::npos);
    ASSERT_TRUE(content.find("error msg") != std::string::npos);

    std::remove(path.c_str());
}

TEST(stats_tracking) {
    AsyncLogger logger;
    const auto& s1 = logger.stats();
    ASSERT_EQ(s1.messagesQueued.load(), (uint64_t)0);
    ASSERT_EQ(s1.messagesWritten.load(), (uint64_t)0);

    for (int i = 0; i < 5; ++i) {
        logger.info("test");
    }
    logger.flush();

    const auto& s2 = logger.stats();
    ASSERT_EQ(s2.messagesQueued.load(), (uint64_t)5);
    ASSERT_EQ(s2.messagesWritten.load(), (uint64_t)5);

    logger.shutdown();
}

TEST(after_shutdown_logs_are_dropped) {
    AsyncLogger logger;
    logger.shutdown();
    logger.info("after shutdown");
    const auto& stats = logger.stats();
    ASSERT_EQ(stats.messagesQueued.load(), (uint64_t)0);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "AsyncLogger Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
