# ThreadPool API (v0.4.0+)

C++23 work-stealing thread pool with stop tokens, backpressure, and
statistics.

## Header

```cpp
#include "thread/ThreadPool.h"
```

## Namespace

```cpp
namespace OmniNet::Thread {
    class ThreadPool;
    using Task = std::move_only_function<void()>;
}
```

## Quick Start

```cpp
#include "thread/ThreadPool.h"
using namespace OmniNet;
using namespace OmniNet::Thread;

int main() {
    // Default config: hardware_concurrency() workers, 1024 max queue
    ThreadPool pool;
    
    // Submit a task
    auto fut = pool.submit([]() { return 42; });
    int result = fut.get();  // Blocks until done
    
    return 0;
}
```

## Configuration

```cpp
struct Config {
    size_t workerCount = 0;        // 0 = hardware_concurrency()
    size_t maxQueueSize = 1024;   // Backpressure threshold
    bool stealWork = true;         // Work stealing between workers
    size_t spinCount = 1000;       // (Internal, reserved)
};

ThreadPool::Config cfg;
cfg.workerCount = 8;
cfg.maxQueueSize = 10000;
ThreadPool pool(cfg);
```

## API Reference

### Construction

```cpp
// Default constructor (uses default config)
ThreadPool pool;

// With explicit config
ThreadPool pool(cfg);

// Movable but not copyable
auto pool2 = std::move(pool);  // OK
ThreadPool pool3 = pool;       // Error: copy not allowed
```

### Task Submission

```cpp
// submit(callable) - returns std::future<result>
auto fut = pool.submit([]() { return 42; });
int result = fut.get();

// submit with arguments
auto fut = pool.submit([](int a, int b) { return a + b; }, 3, 4);
int sum = fut.get();

// submit(Task) - non-blocking, returns false if queue full
bool accepted = pool.try_submit(Thread::Task{[]() {
    // ... do work ...
}});
```

### Delayed Tasks

```cpp
// submit_after(delay, callable) - returns std::future<void>
auto fut = pool.submit_after(2s, []() {
    std::cout << "Run after 2 seconds\n";
});
fut.wait();
```

### Statistics

```cpp
struct Stats {
    size_t workerCount;
    size_t activeTasks;
    size_t queuedTasks;
    uint64_t totalExecuted;
    uint64_t totalStolen;
    uint64_t totalRejected;
};

auto stats = pool.stats();
std::cout << "Active: " << stats.activeTasks << "\n"
          << "Queued: " << stats.queuedTasks << "\n"
          << "Total: " << stats.totalExecuted << "\n"
          << "Stolen: " << stats.totalStolen << "\n"
          << "Rejected: " << stats.totalRejected << "\n";
```

### Lifecycle

```cpp
bool running = pool.running();  // Check if running

pool.shutdown();  // Wait for in-flight tasks, then stop
pool.request_stop();  // Request cancellation, don't wait

// Destructor also calls shutdown
{
    ThreadPool pool;
    // ... use ...
}  // ~ThreadPool() waits for in-flight tasks
```

## Performance

From `benchmarks/thread_pool_bench`:

| Workers | Tasks | Throughput | Per Task |
|---------|-------|------------|----------|
| 1 | 1,000 | 490k tasks/s | 2µs |
| 2 | 10,000 | ~900k tasks/s | 1.1µs |
| 4 | 100,000 | ~3M tasks/s | 330ns |

**Tip**: Use the right number of workers. Too few = underutilized CPUs.
Too many = context switching overhead. Start with `hardware_concurrency()`.

## Patterns

### Producer-Consumer

```cpp
ThreadPool pool;
std::atomic<int> processed{0};

// Producers
for (int i = 0; i < 100; ++i) {
    pool.submit([&processed, i]() {
        // Process item i
        processed.fetch_add(1);
    });
}

// Wait for all
while (processed.load() < 100) {
    std::this_thread::sleep_for(1ms);
}
```

### Map-Reduce

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};

// Map
std::vector<std::future<int>> futures;
for (int x : data) {
    futures.push_back(pool.submit([x]() { return x * x; }));
}

// Reduce
int sum = 0;
for (auto& f : futures) {
    sum += f.get();
}
// sum = 1 + 4 + 9 + 16 + 25 = 55
```

### Pipeline

```cpp
ThreadPool pool;

// Stage 1: Parse
auto parseFut = pool.submit([data]() {
    return parseData(data);
});

// Stage 2: Process (uses stage 1's result)
auto processFut = pool.submit([&pool, &parseFut]() {
    auto data = parseFut.get();
    return processData(data);
});

// Stage 3: Store
pool.submit([&processFut]() {
    auto result = processFut.get();
    storeResult(result);
});
```

## Backpressure

When the queue is full, `submit()` **blocks** until space is available:

```cpp
// This blocks if queue is full
auto fut = pool.submit([]() { /* ... */ });

// This returns false if queue is full (non-blocking)
bool ok = pool.try_submit([]() { /* ... */ });
if (!ok) {
    // Apply backpressure: drop, queue, or shed load
}
```

## Stop Tokens

`std::jthread` provides cooperative cancellation. Workers can check
`st.stop_requested()`:

```cpp
pool.submit([st = pool.stop_token()]() {
    while (!st.stop_requested()) {
        // do work
    }
});
```

## Thread Safety

All ThreadPool methods are **thread-safe**:
- Multiple threads can submit concurrently
- `stats()` is atomic (no torn reads)
- Shutdown is safe to call from any thread

## Common Pitfalls

1. **Recursion deadlock**: Don't `fut.get()` on a task that itself submits
   more tasks to the same pool — deadlocks if all workers are blocked.

2. **Future lifetime**: The `std::future` returned by `submit()` becomes
   invalid after destruction. Wait or detach before the future goes out of
   scope.

3. **Exception in task**: Uncaught exceptions are logged to stderr but
   don't propagate. The task is marked complete regardless.

4. **Pool destruction with pending tasks**: The destructor calls
   `shutdown()` which waits for in-flight tasks. Don't use `request_stop()`
   in destructors if you want tasks to complete.

## See Also

- [`docs/api/async.md`](async.md) - Coroutines built on top of ThreadPool
- [`docs/api/expected.md`](expected.md) - Error handling for async operations
- `tests/test_thread_pool.cpp` - 16 test cases
- `benchmarks/thread_pool_bench.cpp` - Performance validation
