// OmniNet ThreadPool Throughput Benchmark
// Validates ThreadPool task submission throughput and latency
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "thread/ThreadPool.h"

using namespace OmniNet;
using namespace OmniNet::Thread;
using namespace std::chrono_literals;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "ThreadPool Throughput Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    for (size_t numWorkers : {1, 2, 4, 8}) {
        for (size_t numTasks : {1000, 10000, 100000}) {
            std::cout << "--- " << numWorkers << " workers, "
                      << numTasks << " tasks ---\n" << std::endl;

            ThreadPool::Config cfg;
            cfg.workerCount = numWorkers;
            ThreadPool pool(cfg);

            // Warmup
            for (size_t i = 0; i < 100; ++i) {
                pool.submit([]() { return 42; }).wait();
            }

            // Measure pure submit throughput
            auto start = std::chrono::steady_clock::now();
            std::vector<std::future<void>> futures;
            futures.reserve(numTasks);
            for (size_t i = 0; i < numTasks; ++i) {
                futures.push_back(pool.submit([]() {
                    // Empty task to measure pure overhead
                }));
            }
            for (auto& f : futures) f.wait();
            auto end = std::chrono::steady_clock::now();

            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double nsPerTask = static_cast<double>(ns) / numTasks;
            double tasksPerSec = 1e9 / nsPerTask;

            std::cout << "  Total:      " << ns << " ns\n"
                      << "  Per task:   " << nsPerTask << " ns\n"
                      << "  Throughput: " << static_cast<size_t>(tasksPerSec)
                      << " tasks/s\n" << std::endl;

            // Measure work throughput (each task computes something)
            std::atomic<uint64_t> counter{0};
            auto start2 = std::chrono::steady_clock::now();
            std::vector<std::future<void>> futures2;
            for (size_t i = 0; i < numTasks; ++i) {
                futures2.push_back(pool.submit([&counter]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }));
            }
            for (auto& f : futures2) f.wait();
            auto end2 = std::chrono::steady_clock::now();

            auto ns2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
            std::cout << "  With work:  " << ns2 << " ns total, "
                      << static_cast<size_t>(counter.load()) << " tasks completed\n"
                      << "  Per task:   " << static_cast<double>(ns2) / numTasks
                      << " ns\n" << std::endl;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ThreadPool validated" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
