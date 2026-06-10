# OmniNet Benchmarks

Performance benchmarks validating the v0.4.0+ refactor claims.

## Available Benchmarks

| Benchmark | Purpose | Target |
|-----------|---------|--------|
| `logger_throughput_bench` | AsyncLogger msgs/sec | 1M+ msg/s |
| `expected_overhead_bench` | std::expected vs exceptions | 10x+ faster |
| `thread_pool_bench` | Task submission throughput | varies |
| `coroutine_overhead_bench` | Coroutine vs regular function overhead | baseline |

## Build

```bash
cmake -S . -B build
cmake --build build --target logger_throughput_bench
cmake --build build --target expected_overhead_bench
cmake --build build --target thread_pool_bench
cmake --build build --target coroutine_overhead_bench
```

## Run

```bash
./build/benchmarks/logger_throughput_bench
./build/benchmarks/expected_overhead_bench
./build/benchmarks/thread_pool_bench
./build/benchmarks/coroutine_overhead_bench
```

## Results Summary

Run the benchmarks to get current results. Targets from the plan:

| Metric | v0.4.0 | v0.5.0 Target |
|--------|--------|---------------|
| AsyncLogger throughput | 10k msg/s | **1M+ msg/s** |
| std::expected (success) | ~ns | **~ns** (10x faster than exceptions) |
| std::expected (error) | ~µs | **~ns** (1000x faster than exceptions) |
| ThreadPool task submission | varies | **>1M tasks/s** |
| Coroutine overhead | varies | **<100ns** per call |
