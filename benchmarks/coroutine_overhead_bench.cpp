// OmniNet Coroutine Overhead Benchmark
// Measures the overhead of C++20 coroutines vs regular function calls
#include <chrono>
#include <iostream>

#include "async/Task.h"

using namespace OmniNet;
using namespace OmniNet::Async;
using namespace std::chrono_literals;

// Coroutine version
Task<int> coroReturn() {
    co_return 42;
}

// Coroutine chain
Task<int> coroChain() {
    int v = co_await coroReturn();
    co_return v + 1;
}

// Regular function version
int regularReturn() {
    return 42;
}

int regularChain() {
    int v = regularReturn();
    return v + 1;
}

template<typename Func>
double benchmark(const std::string& name, size_t iterations, Func&& func) {
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func(i);
    }
    auto end = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double nsPerOp = static_cast<double>(ns) / iterations;
    std::cout << "  " << name << ":\n"
              << "    Total:     " << ns << " ns\n"
              << "    Per op:    " << nsPerOp << " ns\n"
              << "    Relative:  " << static_cast<size_t>(nsPerOp / 1) << "ns baseline\n";
    return nsPerOp;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coroutine Overhead Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    constexpr size_t ITERATIONS = 100'000;

    std::cout << "--- Single call ---\n" << std::endl;
    benchmark("Regular function", ITERATIONS, [](size_t) {
        int r = regularReturn();
        (void)r;
    });

    benchmark("Coroutine (with resume + destroy)", ITERATIONS, [](size_t) {
        Task<int> t = coroReturn();
        t.resume();
        int r = std::move(t).result();
        (void)r;
    });

    std::cout << "\n--- Two-call chain ---\n" << std::endl;
    benchmark("Regular function chain", ITERATIONS, [](size_t) {
        int r = regularChain();
        (void)r;
    });

    benchmark("Coroutine chain (with awaits)", ITERATIONS, [](size_t) {
        Task<int> t = coroChain();
        t.resume();
        int r = std::move(t).result();
        (void)r;
    });

    std::cout << "\n========================================" << std::endl;
    std::cout << "Coroutines have overhead but enable async composition" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
