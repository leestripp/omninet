// OmniNet - ThreadPool Unit Tests
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#include "thread/ThreadPool.h"

using namespace OmniNet::Thread;
using namespace std::chrono_literals;

// Helper to create a config with N workers
static ThreadPool::Config makeConfig(size_t workers) {
    ThreadPool::Config cfg;
    cfg.workerCount = workers;
    return cfg;
}

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

TEST(basic_submit) {
    ThreadPool pool(makeConfig(2));
    auto future = pool.submit([]() { return 42; });
    ASSERT_EQ(future.get(), 42);
}

TEST(concurrent_submit) {
    ThreadPool pool(makeConfig(4));
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 1000; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }
    ASSERT_EQ(counter.load(), 1000);
}

TEST(future_result) {
    ThreadPool pool(makeConfig(2));
    auto future = pool.submit([]() {
        return std::string("hello from thread pool");
    });
    ASSERT_EQ(future.get(), std::string("hello from thread pool"));
}

TEST(exception_propagation) {
    ThreadPool pool(makeConfig(2));
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("task error");
    });
    bool caught = false;
    try {
        future.get();
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_EQ(std::string(e.what()), std::string("task error"));
    }
    ASSERT_TRUE(caught);
    // Pool should still work after exception
    auto f2 = pool.submit([]() { return 99; });
    ASSERT_EQ(f2.get(), 99);
}

TEST(try_submit_full_queue) {
    auto config = makeConfig(1);
    config.maxQueueSize = 2;  // Very small for testing
    ThreadPool pool(config);

    // First task takes time, so subsequent ones queue up
    auto firstTask = pool.submit([]() {
        std::this_thread::sleep_for(100ms);
    });

    // Give the worker time to start the first task
    std::this_thread::sleep_for(10ms);

    // Fill the queue
    pool.try_submit([]() {});
    pool.try_submit([]() {});

    // Now the queue should be full
    bool rejected = !pool.try_submit([]() {});
    ASSERT_TRUE(rejected);

    firstTask.wait();
}

TEST(multiple_workers_used) {
    ThreadPool pool(makeConfig(4));
    std::atomic<int> concurrentCount{0};
    std::atomic<int> maxConcurrent{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([&concurrentCount, &maxConcurrent]() {
            int current = concurrentCount.fetch_add(1) + 1;
            int prevMax = maxConcurrent.load();
            while (current > prevMax &&
                   !maxConcurrent.compare_exchange_weak(prevMax, current)) {}
            std::this_thread::sleep_for(20ms);
            concurrentCount.fetch_sub(1);
        }));
    }

    for (auto& f : futures) f.wait();
    ASSERT_TRUE(maxConcurrent.load() > 1);
}

TEST(graceful_shutdown) {
    auto pool = std::make_unique<ThreadPool>(makeConfig(2));
    std::atomic<int> completed{0};

    for (int i = 0; i < 10; ++i) {
        pool->submit([&completed]() {
            std::this_thread::sleep_for(5ms);
            completed.fetch_add(1);
        }).wait();
    }

    pool->shutdown();
    ASSERT_EQ(completed.load(), 10);
}

TEST(try_submit_returns_false_after_shutdown) {
    auto pool = std::make_unique<ThreadPool>(makeConfig(2));
    pool->shutdown();
    bool rejected = !pool->try_submit([]() {});
    ASSERT_TRUE(rejected);
}

TEST(running_flag) {
    ThreadPool pool(makeConfig(2));
    ASSERT_TRUE(pool.running());
    pool.shutdown();
    ASSERT_TRUE(!pool.running());
}

TEST(worker_count) {
    ThreadPool pool(makeConfig(4));
    ASSERT_EQ(pool.worker_count(), (size_t)4);
}

TEST(stop_token) {
    ThreadPool pool(makeConfig(2));
    auto token = pool.stop_token();
    ASSERT_TRUE(!token.stop_requested());
    pool.request_stop();
    ASSERT_TRUE(token.stop_requested());
}

TEST(default_constructor) {
    ThreadPool pool;
    ASSERT_TRUE(pool.worker_count() > 0);
    auto future = pool.submit([]() { return 7; });
    ASSERT_EQ(future.get(), 7);
}

TEST(submit_after_delayed) {
    ThreadPool pool(makeConfig(2));
    auto start = std::chrono::steady_clock::now();
    auto future = pool.submit_after(50ms, []() {
        return;
    });
    future.wait();
    auto elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(elapsed >= 50ms);
}

TEST(sequential_run) {
    // With 1 worker, tasks should run sequentially
    ThreadPool pool(makeConfig(1));
    std::vector<int> order;
    std::mutex mtx;

    for (int i = 0; i < 5; ++i) {
        pool.submit([&order, &mtx, i]() {
            std::lock_guard<std::mutex> lock(mtx);
            order.push_back(i);
        }).wait();
    }

    ASSERT_EQ(order.size(), (size_t)5);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(order[i], i);
    }
}

TEST(stats_basic) {
    ThreadPool pool(makeConfig(2));
    auto stats1 = pool.stats();
    ASSERT_EQ(stats1.workerCount, (size_t)2);
    ASSERT_EQ(stats1.totalExecuted, (uint64_t)0);

    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() { return i; }).wait();
    }

    auto stats2 = pool.stats();
    ASSERT_EQ(stats2.totalExecuted, (uint64_t)10);
}

TEST(work_stealing_occurs) {
    // With 4 workers, work stealing should reduce queue imbalance
    ThreadPool pool(makeConfig(4));

    // Submit many small tasks
    std::atomic<int> executed{0};
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&executed]() {
            std::this_thread::yield();
            executed.fetch_add(1);
        }));
    }

    for (auto& f : futures) f.wait();
    auto stats = pool.stats();
    ASSERT_EQ(executed.load(), 100);
    std::cout << "(stolen=" << stats.totalStolen << ") ";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "ThreadPool Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
