# Async I/O API (v0.4.0+)

Non-blocking I/O using Linux epoll. `IoContext` is the event loop, and
`AsyncSocket` / `AsyncFile` are coroutine-friendly I/O wrappers.

## Header

```cpp
#include "io/IoContext.h"
#include "io/EpollContext.h"
#include "io/AsyncSocket.h"
#include "io/AsyncFile.h"
```

## Namespace

```cpp
namespace OmniNet::IO {
    class IoContext;        // Abstract base
    class EpollIoContext;  // Linux epoll implementation
    class AsyncSocket;
    class AsyncFile;

    enum class EventType {
        None, Read, Write, Error, Hangup
    };
}
```

## IoContext (Event Loop)

The `IoContext` is the central event multiplexer. Currently only Linux
epoll is implemented (via `EpollIoContext`).

### Quick Start

```cpp
#include "io/EpollContext.h"
#include "thread/ThreadPool.h"
using namespace OmniNet;

int main() {
    Thread::ThreadPool pool({.workerCount = 4});
    ThreadPoolExecutor exec(pool);
    EpollIoContext ctx(exec);

    // Run until stop is called
    ctx.run();

    // Or run one iteration (non-blocking)
    ctx.run_once(100ms);

    // Schedule a callback
    ctx.post([]() {
        std::cout << "Hello from event loop!\n";
    });
}
```

### API Reference

```cpp
class IoContext {
public:
    virtual ~IoContext() = default;
    
    // Run event loop (blocking, returns when stopped)
    virtual void run() = 0;
    virtual void run(std::stop_token st) = 0;
    
    // Run one iteration (returns time spent waiting)
    virtual std::chrono::milliseconds run_once(
        std::chrono::milliseconds timeout) = 0;
    
    // Stop the event loop
    virtual void stop() = 0;
    
    // Schedule a callback
    virtual void post(Async::Task<> task) = 0;
    virtual void resume(std::coroutine_handle<> handle) = 0;
    
    // Watch a socket for events
    virtual uint64_t watch(Socket& sock, EventType events,
                           EventCallback callback) = 0;
    virtual void unwatch(uint64_t watchId) = 0;
    
    // Timers
    virtual uint64_t timer_after(std::chrono::milliseconds delay,
                                  std::function<void()> callback) = 0;
    virtual void timer_cancel(uint64_t timerId) = 0;
    
    virtual Async::Executor& executor() = 0;
    virtual bool running() const = 0;
};
```

## AsyncSocket (Coroutine I/O)

```cpp
class AsyncSocket {
public:
    AsyncSocket(IoContext& ctx, std::unique_ptr<Socket> socket);
    
    // Async read (returns bytes read, 0 = EOF)
    Async::Task<size_t> read_some(std::span<uint8_t> buf);
    
    // Async write (returns bytes written)
    Async::Task<size_t> write_some(std::span<const uint8_t> data);
    
    // Async write all (loops on partial writes)
    Async::Task<void> write_all(std::span<const uint8_t> data);
    
    Socket& native() { return *m_socket; }
    int fd() const { return m_socket->fd(); }
    void close();
};
```

### Usage

```cpp
Task<void> handleConnection(EpollIoContext& ctx, Socket sock) {
    AsyncSocket asock(ctx, std::make_unique<Socket>(std::move(sock)));
    
    std::array<uint8_t, 4096> buf;
    while (true) {
        size_t n = co_await asock.read_some(buf);
        if (n == 0) break;  // EOF
        co_await asock.write_all({buf.data(), n});  // Echo
    }
}
```

## Awaitable Socket Events

```cpp
// Wait until socket is readable
co_await when_readable(ctx, socket);

// Wait until socket is writable
co_await when_writable(ctx, socket);
```

## AsyncFile (Async File I/O)

Cross-platform file I/O using thread pool. Useful for static file serving
without blocking the event loop.

```cpp
class AsyncFile {
public:
    explicit AsyncFile(Async::Executor& exec);
    
    // Open file
    Async::Task<> open(std::string_view path, std::string_view mode = "r");
    
    // Read entire file
    Async::Task<std::vector<uint8_t>> read_all();
    
    // Read range
    Async::Task<std::vector<uint8_t>> read_range(size_t offset, size_t length);
    
    // Write
    Async::Task<size_t> write(std::span<const uint8_t> data);
    Async::Task<size_t> write_at(size_t offset, std::span<const uint8_t> data);
    
    // Close
    Async::Task<> close();
    
    bool isOpen() const;
    size_t size() const;
};
```

### Usage

```cpp
Task<Response> serveStatic(EpollIoContext& ctx, std::string path) {
    AsyncFile file(ctx.executor());
    co_await file.open(path);
    
    if (!file.isOpen()) {
        co_return Response::notFound();
    }
    
    auto data = co_await file.read_all();
    file.close().resume();
    
    Response resp;
    resp.setBody(std::move(data));
    co_return resp;
}
```

## Architecture

```
┌─────────────────┐
│  AsyncSocket    │  ← coroutine interface
│  AsyncFile      │
└────────┬────────┘
         │ co_await when_readable/writable
┌────────▼────────┐
│  IoContext      │  ← epoll event loop
│  (epoll_wait)   │
└────────┬────────┘
         │
┌────────▼────────┐
│  ThreadPool     │  ← handler execution
└─────────────────┘
```

## See Also

- [`docs/api/threadpool.md`](threadpool.md) - Underlying executor
- [`docs/api/async.md`](async.md) - Coroutines
- [`docs/api/async-server.md`](async-server.md) - HTTP server
- `tests/test_io_context.cpp` - 7 test cases
- `tests/test_async_file.cpp` - 12 test cases
