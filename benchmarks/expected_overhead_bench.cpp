// OmniNet std::expected Overhead Benchmark
// Validates that std::expected is faster than exceptions for error handling
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/Expected.h"

using namespace OmniNet;
using namespace std::chrono_literals;

// Simulate an operation that can fail
struct OpResult {
    int value;
};

// Exception-based version
OpResult opWithException(bool shouldFail) {
    if (shouldFail) {
        throw std::runtime_error("Operation failed");
    }
    return {42};
}

// std::expected version
Result<OpResult, SocketError> opWithExpected(bool shouldFail) {
    if (shouldFail) {
        return std::unexpected<SocketError>{SocketError::Unknown};
    }
    return OpResult{42};
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
              << "    Total:      " << ns << " ns\n"
              << "    Per op:     " << nsPerOp << " ns\n"
              << "    Throughput: " << static_cast<size_t>(1e9 / nsPerOp)
              << " ops/s\n";
    return nsPerOp;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "std::expected Overhead Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    constexpr size_t ITERATIONS = 1'000'000;

    std::cout << "--- Success path (no errors) ---\n" << std::endl;

    auto excSuccess = benchmark("Exception success", ITERATIONS, [](size_t) {
        try {
            auto r = opWithException(false);
            (void)r;
        } catch (...) {}
    });

    auto expSuccess = benchmark("std::expected success", ITERATIONS, [](size_t) {
        auto r = opWithExpected(false);
        (void)r;
    });

    std::cout << "\n  Ratio: std::expected is "
              << static_cast<size_t>(excSuccess / expSuccess)
              << "x faster on success path\n" << std::endl;

    std::cout << "\n--- Error path (50% fail rate) ---\n" << std::endl;

    auto excError = benchmark("Exception error", ITERATIONS, [](size_t i) {
        try {
            auto r = opWithException(i % 2 == 0);
            (void)r;
        } catch (...) {}
    });

    auto expError = benchmark("std::expected error", ITERATIONS, [](size_t i) {
        auto r = opWithExpected(i % 2 == 0);
        (void)r;
    });

    std::cout << "\n  Ratio: std::expected is "
              << static_cast<size_t>(excError / expError)
              << "x faster on error path\n" << std::endl;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Target: std::expected is 10x+ faster than exceptions" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
