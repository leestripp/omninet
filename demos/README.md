# OmniNet Async Server Demo (v0.5.0)

A comprehensive integration demo showcasing all the new v0.4.0+ features
working together in a single HTTP server.

## Features Showcased

- **AsyncServer** — non-blocking HTTP server using epoll event loop
- **AsyncDatabase** — coroutine-friendly SQLite wrapper
- **DatabasePool** — connection pooling for concurrent access
- **AsyncLogger** — non-blocking logging with MPSC queue
- **AsyncFile** — cross-platform async file I/O via thread pool
- **std::expected<T, E>** — modern type-safe error handling
- **C++23 coroutines** — `co_await` for all async operations

## Endpoints

| Method | Path                | Description |
|--------|---------------------|-------------|
| GET    | `/`                 | Server info |
| GET    | `/api/users`        | List all users (DB query via pool) |
| GET    | `/api/users/:id`    | Get single user (with error handling) |
| GET    | `/api/stats`        | Server statistics |
| GET    | `/api/logtest`      | Test logger with 1000 messages |
| GET    | `/static/*`         | Static files served via AsyncFile |

## Build

```bash
cmake -S . -B build
cmake --build build --target async_server_demo
```

## Run

```bash
./build/demos/async_server_demo
```

The server starts on `127.0.0.1:19080` by default. Try:

```bash
curl http://localhost:19080/
curl http://localhost:19080/api/users
curl http://localhost:19080/api/users/1
curl http://localhost:19080/api/stats
curl http://localhost:19080/api/logtest
curl http://localhost:19080/static/hello.txt
curl http://localhost:19080/static/index.html
```

## Code Highlights

### Coroutine Handler
```cpp
server.get("/api/users", Async::AsyncHandler{
    [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
        auto pool = ...;
        auto db = co_await pool->acquire_async();
        auto rows = co_await db->query("SELECT * FROM users");
        // ... build response ...
        co_return resp;
    }
});
```

### AsyncFile for Static Serving
```cpp
AsyncFile file(exec);
co_await file.open("static/hello.txt");
auto data = co_await file.read_all();
```

### Non-Blocking Logging
```cpp
logger->info("1000 log messages in " + std::to_string(us) + "µs");
logger->flush();  // Optional - ensures all messages written
```

## Performance

The `/api/logtest` endpoint demonstrates the non-blocking logger by
logging 1000 messages and reporting throughput (typically 1M+ msg/s).
