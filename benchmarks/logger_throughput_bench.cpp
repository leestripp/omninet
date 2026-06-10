// OmniNet Logger Throughput Benchmark
// Validates the claim that AsyncLogger can do 1M+ log messages per second
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "async/AsyncLogger.h"
#include "thread/ThreadPool.h"
#include "utils/Logger.h"

using namespace OmniNet;
using namespace OmniNet::Async;
using namespace std::chrono_literals;

struct TestResult {
    std::string name;
    size_t messages;
    std::chrono::milliseconds elapsed;
    double messagesPerSec() const {
        return static_cast<double>(messages) /
               (static_cast<double>(elapsed.count()) / 1000.0);
    }
    void print() const {
        std::cout << "  " << name << ":\n"
                  << "    Messages:   " << messages << "\n"
                  << "    Elapsed:    " << elapsed.count() << " ms\n"
                  << "    Throughput: " << static_cast<size_t>(messagesPerSec())
                  << " msg/s\n";
    }
};

TestResult benchmarkAsyncLogger(size_t numMessages, size_t numThreads) {
    TestResult result;
    result.name = "AsyncLogger (" + std::to_string(numThreads) + " threads, " +
                  std::to_string(numMessages) + " messages)";

    Thread::ThreadPool::Config poolCfg;
    poolCfg.workerCount = 4;
    Thread::ThreadPool pool(poolCfg);
    AsyncLogger logger(pool);
    logger.setLevel(LogLevel::INFO);

    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    auto start = std::chrono::steady_clock::now();
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < numMessages / numThreads; ++i) {
                logger.info("Benchmark message from thread " +
                           std::to_string(t) + " iteration " +
                           std::to_string(i));
            }
        });
    }

    // Start all threads simultaneously
    startFlag.store(true, std::memory_order_release);

    for (auto& t : threads) t.join();
    logger.flush();

    auto end = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.messages = numMessages;
    return result;
}

TestResult benchmarkSyncLogger(size_t numMessages, size_t numThreads) {
    TestResult result;
    result.name = "Sync Logger (" + std::to_string(numThreads) + " threads, " +
                  std::to_string(numMessages) + " messages)";

    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    auto start = std::chrono::steady_clock::now();
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < numMessages / numThreads; ++i) {
                // Use stderr for sync logger (no file I/O)
                std::cerr << "[INFO] Benchmark message from thread "
                          << t << " iteration " << i << "\n";
            }
        });
    }

    startFlag.store(true, std::memory_order_release);

    for (auto& t : threads) t.join();

    auto end = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.messages = numMessages;
    return result;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Logger Throughput Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Test with different scales
    std::vector<std::pair<size_t, size_t>> configs = {
        {10000, 1},   // 10k msgs, 1 thread
        {100000, 1}, // 100k msgs, 1 thread
        {100000, 4}, // 100k msgs, 4 threads
        {1000000, 4},// 1M msgs, 4 threads
    };

    for (const auto& [numMsgs, numThreads] : configs) {
        std::cout << "\n--- " << numMsgs << " messages, "
                  << numThreads << " thread(s) ---\n" << std::endl;

        // AsyncLogger (non-blocking)
        try {
            auto asyncResult = benchmarkAsyncLogger(numMsgs, numThreads);
            asyncResult.print();
        } catch (const std::exception& e) {
            std::cout << "  AsyncLogger FAILED: " << e.what() << std::endl;
        }

        // Compare with sync (if small enough to be practical)
        if (numMsgs <= 10000) {
            try {
                auto syncResult = benchmarkSyncLogger(numMsgs, numThreads);
                syncResult.print();
            } catch (const std::exception& e) {
                std::cout << "  Sync Logger FAILED: " << e.what() << std::endl;
            }
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "Target: 1M+ msg/s with AsyncLogger" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
