// OmniNet - Async Task Unit Tests
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "async/Task.h"
#include "async/Executor.h"
#include "async/AsyncHandler.h"
#include "thread/ThreadPool.h"

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

// ============================================================================
// Test Coroutines
// ============================================================================

// Simple coroutine that returns a value
static Task<int> simple_task() {
    co_return 42;
}

// Coroutine that awaits another task
static Task<int> chain_task() {
    int v = co_await simple_task();
    co_return v * 2;
}

// Coroutine that throws
static Task<int> throwing_task() {
    throw std::runtime_error("oops");
    // cppcheck-suppress unreachableCode
    co_return 0;  // Unreachable, but required to make this a coroutine
}

// Coroutine that yields and resumes
static Task<void> void_task() {
    co_return;
}

// Coroutine that takes an argument
static Task<int> add_task(int a, int b) {
    co_return a + b;
}

// Coroutine that uses an executor
static Task<int> executor_task(Thread::ThreadPool& pool) {
    auto fut = pool.submit([]() { return 100; });
    int v = fut.get();
    co_return v + 1;
}

// ============================================================================
// Tests
// ============================================================================

TEST(basic_task) {
    Task<int> task = simple_task();
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 42);
}

TEST(chain_task) {
    Task<int> task = chain_task();
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 84);  // 42 * 2
}

TEST(throwing_task) {
    Task<int> task = throwing_task();
    bool caught = false;
    try {
        blocking_get(std::move(task));
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_EQ(std::string(e.what()), std::string("oops"));
    }
    ASSERT_TRUE(caught);
}

TEST(void_task) {
    Task<void> task = void_task();
    blocking_get(std::move(task));
    // If we get here, it worked
}

TEST(task_with_args) {
    Task<int> task = add_task(3, 4);
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 7);
}

TEST(task_awaiter_ready) {
    Task<int> task = simple_task();
    ASSERT_TRUE(!task.is_ready());  // Initially not ready
    blocking_get(std::move(task));
    // After completion, the task is in final_suspend, so technically still
    // "not ready" by the done() check (done returns true)
    // Actually is_ready returns true when done() is true
}

TEST(task_move_only) {
    auto make = []() -> Task<std::string> {
        co_return std::string("hello");
    };
    Task<std::string> a = make();
    Task<std::string> b = std::move(a);  // Should compile (movable)
    ASSERT_TRUE(!b.is_ready());
    std::string result = blocking_get(std::move(b));
    ASSERT_EQ(result, std::string("hello"));
}

TEST(task_destruction_safe) {
    auto make = []() -> Task<int> {
        co_return 1;
    };
    {
        Task<int> task = make();
        // Don't await - let destructor handle it
    }
    // If we get here, destruction worked
}

TEST(multiple_awaits) {
    auto make = []() -> Task<int> {
        co_return 5;
    };
    auto use = [&]() -> Task<int> {
        int a = co_await make();
        int b = co_await make();
        int c = co_await make();
        co_return a + b + c;
    };
    Task<int> task = use();
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 15);
}

TEST(executor_basic) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);

    auto fut = exec.submit([]() { return 99; });
    ASSERT_EQ(fut.get(), 99);
}

TEST(executor_with_task) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 2;
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);

    Task<int> task = executor_task(pool);
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 101);  // 100 + 1
}

TEST(executor_try_submit) {
    Thread::ThreadPool::Config cfg;
    cfg.workerCount = 1;
    cfg.maxQueueSize = 1;  // Small queue for testing
    Thread::ThreadPool pool(cfg);
    ThreadPoolExecutor exec(pool);

    // Submit a long-running task first
    auto longTask = pool.submit([]() {
        std::this_thread::sleep_for(100ms);
        return 1;
    });
    std::this_thread::sleep_for(10ms);  // Let it start

    // Fill the queue
    pool.try_submit(Thread::Task{[]() {}});

    // try_submit should fail because the queue is full
    bool rejected = !exec.try_submit(Thread::Task{[]() {}});
    ASSERT_TRUE(rejected);

    longTask.wait();
}

TEST(handler_signature_compiles) {
    // Just check that the types compile
    AsyncHandler handler = []([[maybe_unused]] const HTTP::Request& req,
                              [[maybe_unused]] Executor& exec) -> Task<HTTP::Response> {
        HTTP::Response resp;
        resp.setBody("hello");
        co_return resp;
    };
    // Don't actually invoke (no HTTP server setup needed for compile test)
    (void)handler;
}

TEST(many_sequential_tasks) {
    auto make = [](int v) -> Task<int> {
        co_return v;
    };
    auto chain = [&]() -> Task<int> {
        int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += co_await make(i);
        }
        co_return sum;
    };
    Task<int> task = chain();
    int result = blocking_get(std::move(task));
    ASSERT_EQ(result, 4950);  // Sum 0..99
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Async Task Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
