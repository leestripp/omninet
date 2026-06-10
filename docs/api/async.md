# Async Tasks API (v0.4.0+)

C++20 coroutines for asynchronous operations. `Task<T>` is the return type
of async functions, and `co_await` suspends until the result is ready.

## Header

```cpp
#include "async/Task.h"
#include "async/Executor.h"
```

## Namespace

```cpp
namespace OmniNet::Async {
    template<typename T> class Task;
    using Task = Task<>;  // Convenience alias
    class Executor;
    class ThreadPoolExecutor;
}
```

## Quick Start

```cpp
#include "async/Task.h"
using namespace OmniNet::Async;

// Define an async function
Task<int> computeValue() {
    co_return 42;
}

// Use it
int main() {
    Task<int> task = computeValue();
    int result = blocking_get(std::move(task));
    // result == 42
}
```

## Task<T>

`Task<T>` is a lazy coroutine return type. The coroutine doesn't start
executing until you call `resume()` or `co_await` it.

```cpp
// Create a task
Task<int> getValue() {
    co_return 42;
}

// Run synchronously (block)
Task<int> task = getValue();
int result = task.result();  // Runs coroutine, blocks until done

// Or use resume + result separately
task.resume();  // Run until suspended or complete
int result = task.result();  // Get result
```

### Awaiting Tasks

`Task<T>` can be awaited from inside another coroutine:

```cpp
Task<int> inner() {
    co_return 42;
}

Task<int> outer() {
    int v = co_await inner();  // Suspends until inner() completes
    co_return v + 1;
}

Task<void> chain() {
    co_await outer();  // Suspends until outer() completes
}
```

### Move Semantics

`Task<T>` is **move-only**:

```cpp
Task<int> make() { co_return 1; }

Task<int> a = make();
Task<int> b = std::move(a);  // OK
Task<int> c = a;             // Error: copy deleted
```

### Lifetime

The `Task<T>` destructor destroys the coroutine frame. **Always** either
await, resume, or destroy a `Task<T>`:

```cpp
// DANGER: leak
Task<int> makeAndLeak() {
    co_return new int(42);  // Leaked!
}

// CORRECT: destroyed properly
{
    Task<int> t = makeAndLeak();
}  // destructor runs coroutine and cleans up
```

## Executor

An `Executor` is an abstract interface for running tasks:

```cpp
class Executor {
public:
    virtual ~Executor() = default;
    
    // Submit a callable, get a future
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    // Submit a Task (move-only)
    virtual bool try_submit(Thread::Task task) = 0;
    
    // Resume a coroutine handle
    virtual void resume(std::coroutine_handle<> handle) = 0;
    
    virtual bool running() const = 0;
    virtual std::stop_token stop_token() const = 0;
};
```

### ThreadPoolExecutor

Concrete implementation backed by a `ThreadPool`:

```cpp
Thread::ThreadPool pool({.workerCount = 4});
ThreadPoolExecutor exec(pool);

// Use as Executor
auto fut = exec.submit([]() { return 42; });
int result = fut.get();

// Dispatch a coroutine
exec.resume(some_coroutine_handle);
```

## Async Handlers

For HTTP servers, `AsyncHandler` is the signature:

```cpp
using AsyncHandler = std::function<Task<Response>(
    const HTTP::Request&,
    Executor&
)>;

// Example
AsyncHandler handler = [](const Request& req, Executor& exec)
    -> Task<Response> {
    auto data = co_await exec.submit([]() {
        return loadFromDisk();
    });
    Response resp;
    resp.setBody(std::move(data));
    co_return resp;
};
```

## Coroutines Best Practices

### 1. Don't Block the Event Loop

```cpp
// BAD: blocks the calling thread
Task<Response> bad_handler() {
    std::this_thread::sleep_for(1s);  // blocks!
    co_return Response::ok();
}

// GOOD: offload to executor
Task<Response> good_handler(Executor& exec) {
    co_await exec.submit([]() {
        std::this_thread::sleep_for(1s);  // blocks worker, not event loop
    });
    co_return Response::ok();
}
```

### 2. Use `co_await` for Async Operations

```cpp
// GOOD: yields control while waiting
Task<Response> good_handler(Executor& exec) {
    auto db = co_await pool.acquire_async();  // suspends
    auto rows = co_await db->query(sql);     // suspends
    // ...
    co_return resp;
}
```

### 3. Handle Errors Explicitly

```cpp
Task<Response> handler_with_errors(Executor& exec) {
    auto dbOpt = co_await pool.acquire_async();
    if (!dbOpt) {
        co_return Response::internalError("DB unavailable");
    }
    // ... use db ...
    co_return Response::ok();
}
```

### 4. Clean Up Resources (RAII)

```cpp
Task<void> use_resource(Executor& exec) {
    // Resource acquired
    auto resource = acquireResource();
    try {
        // ... use resource ...
        co_return;
    } catch (...) {
        releaseResource(resource);
        throw;
    }
    // Or use a guard pattern
}
```

## Performance

From `benchmarks/coroutine_overhead_bench`:

| Operation | Regular fn | Coroutine | Overhead |
|-----------|-----------|-----------|----------|
| Single call | 4 ns | 177 ns | 44x |
| Two-call chain | 7 ns | 420 ns | 60x |

**Why the overhead?** C++20 coroutines use heap-allocated frames. Each
`co_await` and `co_return` involves dynamic dispatch.

**Is it worth it?** Yes, because:
- Enables async composition without callbacks
- Non-blocking I/O in event loop
- Co_await chains compose naturally

**Mitigation**: Use `co_await` for async I/O, regular functions for CPU work.

## Common Pitfalls

### 1. Leaking Coroutines

```cpp
// BAD: coroutine frame leaks
Task<int> make() { co_return expensive_computation(); }
auto t = make();  // Never resumed or awaited!
```

**Solution**: Always await, resume, or destroy the Task.

### 2. Capturing References

```cpp
// BAD: dangling reference
Task<int> bad() {
    int x = 42;
    auto lambda = [&]() { return x; };
    co_return lambda();
}

// GOOD: capture by value or use parameter
Task<int> good() {
    int x = 42;
    co_return [x]() { return x; }();
}
```

### 3. Re-throwing Without Capture

```cpp
Task<int> might_throw() {
    throw std::runtime_error("oops");
    co_return 0;  // Unreachable
}

// Caller should handle the exception
Task<int> caller() {
    try {
        int v = co_await might_throw();
        co_return v;
    } catch (const std::exception& e) {
        std::cerr << "Got: " << e.what() << "\n";
        co_return -1;
    }
}
```

## See Also

- [`docs/api/threadpool.md`](threadpool.md) - Underlying executor
- [`docs/api/expected.md`](expected.md) - Error handling patterns
- `tests/test_async_task.cpp` - 14 test cases
- `benchmarks/coroutine_overhead_bench.cpp` - Performance validation
