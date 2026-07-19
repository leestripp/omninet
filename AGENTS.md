# OmniNet — Agent Guide

This file is a concise, factual reference for AI coding agents working on the OmniNet project. OmniNet is a C++ HTTP/WebSocket server library with TLS, SQLite-backed persistence, an admin dashboard, and an async/coroutine-based runtime. All information below is derived from the actual repository contents.

## 1. Project Overview

OmniNet is a modern C++ server framework that provides:

- HTTP/1.1 server with keep-alive, routing, middleware, CORS, and static file serving.
- WebSocket server and client (RFC 6455).
- TLS/HTTPS and WSS via OpenSSL.
- SQLite3 database wrapper with prepared statements, transactions, migrations, and an async connection pool.
- Session tokens and scoped API keys stored in SQLite.
- Admin dashboard served from `/omninet` with user management, logs, and stats.
- Rate limiting, intrusion detection, and SSE (Server-Sent Events).
- Public header-only test framework for consumers.
- CLI admin tools for initialization, certificates, users, logs, monitoring, etc.

The project exposes a static library `OmniNet`, a set of test executables, demo applications, benchmarks, and command-line tools.

## 2. Technology Stack

| Layer | Technology |
|-------|------------|
| Language | C++23 (required) |
| Build system | CMake 3.16+ |
| Default compiler | Clang/Clang++ if available; GCC 13+ and MSVC 19.36+ also supported |
| TLS | OpenSSL 1.1.1+ (3.x recommended) |
| Database | SQLite3 |
| Compression | zlib (optional, `OMNINET_HAVE_ZLIB`) |
| JSON | nlohmann/json v3.11.3 (fetched via CMake FetchContent) |
| Vector DB | sqlite-vec v0.1.6 (fetched via CMake FetchContent, optional via `OMNINET_ENABLE_VEC`) |
| Threading | `pthread`, `std::jthread`, C++20 coroutines |
| Static analysis | clang-tidy (`.clang-tidy`) |

C++23 is mandatory. The build will fail if the compiler does not support `-std=c++23` (or `/std:c++latest` on MSVC) and coroutines.

## 3. Repository Layout

```
/
├── CMakeLists.txt          # Root build configuration
├── .clang-tidy             # clang-tidy configuration
├── .gitignore
├── README.md               # User-facing quick-start and feature list
├── prompt.txt              # Active feature/bug task description (read it)
├── cmake/
│   └── FindCppcheck.cmake  # Legacy CMake finder (cppcheck removed in v0.9.0)
├── include/OmniNet/
│   └── Test.h              # Public header-only test framework for consumers
├── src/                    # Library sources and internal headers
│   ├── OmniNet.h           # Single include for public API
│   ├── OmniNet.cpp         # Global init/cleanup
│   ├── admin/              # Admin endpoint handler
│   ├── async/              # Coroutine tasks, executors, awaitables, async logger/TLS
│   ├── config/             # Configuration singleton and config structs
│   ├── crypto/             # Crypto utilities and bcrypt password hashing
│   ├── dashboard/          # Dashboard UI handler and rate limiter
│   ├── db/                 # SQLite wrapper, session/user stores, async DB, pool
│   ├── http/               # Request, response, router, server, client, static handler
│   ├── intrusion/          # Intrusion detector
│   ├── io/                 # epoll-based IoContext, AsyncSocket, AsyncFile
│   ├── net/                # Socket, TLSSocket, Buffer
│   ├── server/             # AsyncServer (coroutine-based HTTP server)
│   ├── sse/                # Server-Sent Events handler
│   ├── thread/             # Work-stealing thread pool
│   ├── token/              # Session tokens and API keys
│   ├── utils/              # Logger, LogReader, Exception, Expected, Utils
│   └── vector/             # sqlite-vec VectorStore wrapper
├── tests/                  # Test executables and CMakeLists.txt
├── demos/                  # Example applications
├── benchmarks/             # Performance benchmarks
├── tools/                  # CLI tools
│   ├── tools_common/       # Static library shared by tools
│   ├── omninet-certs/      # TLS certificate generation
│   ├── omninet-ctl/        # Server control
│   ├── omninet-db/         # Database operations
│   ├── omninet-firewall/   # Firewall rules
│   ├── omninet-health/     # Health check
│   ├── omninet-init/       # Initialize config/admin user
│   ├── omninet-log/        # Log viewer
│   ├── omninet-mon/        # Monitoring
│   ├── omninet-status/     # System status
│   └── omninet-users/      # User management
├── docs/                   # Markdown API docs and migration guides
├── static/                 # Static files for demos
└── build/                  # CMake build output (gitignored)
```

## 4. Build and Test Commands

### Configure

```bash
# Default configuration (clang preferred automatically)
cmake -S . -B build

# Force GCC
cmake -S . -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

# Disable tests
cmake -S . -B build -DBUILD_TESTING=OFF

# Disable vector DB support
cmake -S . -B build -DOMNINET_ENABLE_VEC=OFF

# Disable gzip compression
cmake -S . -B build -DOMNINET_HAVE_ZLIB=OFF
```

### Build

```bash
# Build everything (library, tests, demos, benchmarks, tools)
cmake --build build -j$(nproc)

# Build specific targets
cmake --build build --target OmniNet
cmake --build build --target omnet_test
cmake --build build --target http_server_demo
cmake --build build --target omninet-init
```

### Test

```bash
# Run all tests via CTest
ctest --test-dir build --output-on-failure

# Run individual test executables directly
./build/tests/omnet_test
./build/tests/test_config
./build/tests/test_async_task
./build/tests/test_thread_pool
# ... other test_* executables
```

### Static Analysis

If `clang-tidy` is installed, CMake creates these targets:

```bash
cmake --build build --target clang_tidy          # All trees
cmake --build build --target clang_tidy_lib      # src/
cmake --build build --target clang_tidy_tests    # tests/
cmake --build build --target clang_tidy_demos    # demos/
cmake --build build --target clang_tidy_tools    # tools/
# Alias:
cmake --build build --target static_analysis
```

`clang-tidy` reads `compile_commands.json`, which CMake generates automatically (`CMAKE_EXPORT_COMPILE_COMMANDS=ON`).

## 5. Code Organization and Architecture

### Namespace Conventions

- Top-level project namespace: `OmniNet`
- Sub-namespaces by module:
  - `OmniNet::HTTP` — request/response/server/client/router
  - `OmniNet::WebSocket` — frames, handshake, server/client
  - `OmniNet::DB` — database wrapper and stores
  - `OmniNet::Async` — coroutine tasks, executors, awaitables
  - `OmniNet::Thread` — thread pool
  - `OmniNet::IO` — event loop and async sockets/files
  - `OmniNet::Server` — `AsyncServer`
  - `OmniNet::Token` — session tokens and API keys
  - `OmniNet::Dashboard` / `OmniNet::Admin` — dashboard and admin endpoints
  - `OmniNet::SSE` — Server-Sent Events
  - `OmniNet::Tools` — CLI tool helpers

### Key Design Patterns

- **Singletons for global state**: `Config::instance()`, `Logger::instance()`.
- **Exception hierarchy**: `OmniNet::Exception` base with I/O, HTTP, DB, auth, validation subclasses in `src/utils/Exception.h`.
- **Type-safe errors**: `std::expected` is used for new code via `OmniNet::Result<T,E>` and `OmniNet::VoidResult<E>` in `src/utils/Expected.h`.
- **Coroutines**: `Async::Task<T>` is the custom lazy/symmetric coroutine return type (`src/async/Task.h`). Async I/O uses `IO::IoContext`/`EventAwaitable`.
- **Thread pool**: Work-stealing `Thread::ThreadPool` with `std::jthread` workers.
- **Two server APIs**:
  - `OmniNet::HTTP::Server` — traditional threaded/blocking HTTP server.
  - `OmniNet::Server::AsyncServer` — coroutine/event-loop-based HTTP server.

### Source/Header Pairing

Most modules have a `.h` and `.cpp` file in the same directory. Internal-only code may be header-only (e.g., `async/Awaitable.h`, `thread/WorkStealingQueue.h`). The public API is exposed through `src/OmniNet.h` and `include/OmniNet/Test.h`.

## 6. Code Style Guidelines

- **Language**: C++23. Use modern features where appropriate (`std::expected`, coroutines, `std::ranges`, deducing `this`).
- **Formatting**: No `.clang-format` file is present in the repository. Follow the existing style in each file.
- **Braces**: Existing code mostly omits braces around single-statement bodies; do not mass-reformat to add them unless required for correctness.
- **Naming**:
  - Types: `PascalCase` (`class Server`, `struct Config`).
  - Functions/methods: `camelCase` (`startAsync()`, `setLogFile()`).
  - Member variables: `m_camelCase` (`m_config`, `m_running`).
  - Namespaces: `PascalCase` (`OmniNet::HTTP`).
- **Comments**: Use `//` line comments. Section banners like `// ============================================================================` are common.
- **Includes**: Prefer `"module/File.h"` for project headers. Use `#pragma once` with a redundant include guard.
- **Deprecated APIs**: Mark old APIs with `[[deprecated("use X instead")]]` rather than removing them immediately.
- **Compiler-specific macros**: Use `#ifdef __GNUC__` guards where needed (e.g., deprecation macros).

### clang-tidy Policy

- Enabled check families: `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`.
- Checks treated as errors: `clang-analyzer-*`, `cppcoreguidelines-init-variables`, null-dereference, undefined-binary-operator.
- Disabled checks (noise): trailing return type, magic numbers, bounds pointer arithmetic, owning memory, special member functions, non-private member variables, identifier length, isolate declaration, cognitive complexity, braces-around-statements.
- Header filter: `'.*/src/.*'`.

## 7. Testing Instructions

### Internal Tests

Tests live in `tests/` and are compiled into separate executables. The main executables are:

- `omnet_test` — broad library test suite in `test_all.cpp`.
- `test_config`, `test_token`, `test_thread_pool`, `test_async_task`, `test_io_context`, `test_async_server`, `test_async_database`, `test_async_logger`, `test_async_tls`, `test_expected`, `test_async_file`, `test_http_connection`, `test_sync_http_server`, `test_http_response`, `test_websocket_broadcast`, `test_websocket_echo`, `test_sse_streaming`, `test_http_client`, `test_buffer_protection`.

Many test files define their own ad-hoc assertion macros (`ASSERT_EQ`, `ASSERT_TRUE`, etc.) rather than using the public framework.

### Public Test Framework

Consumers (and agents writing user-facing examples) can use the header-only framework in `include/OmniNet/Test.h`:

```cpp
#include <OmniNet/Test.h>

OMNINET_TEST(my_test) {
    OMNINET_ASSERT_EQ(1 + 1, 2);
}

int main() {
    return OMNINET_RUN_TESTS();
}
```

Available macros: `OMNINET_TEST`, `OMNINET_REGISTER_TEST`, `OMNINET_RUN_TESTS`, `OMNINET_ASSERT_TRUE/FALSE`, `OMNINET_ASSERT_EQ/NE/GT/GE/LT/LE`, `OMNINET_ASSERT_STR_CONTAINS/NOT_CONTAINS`, `OMNINET_ASSERT_THROWS`, `OMNINET_ASSERT_NO_THROW`, `OMNINET_FAIL`, and expectation variants (`OMNINET_EXPECT_*`).

### Adding Tests

- If the test fits an existing executable, add the test case to that source file.
- For a new focused area, add a new `.cpp` file in `tests/` and register it in `tests/CMakeLists.txt`.
- Link new test executables with `OmniNet` and `pthread` and set `cxx_std_23`.

## 8. Security Considerations

- **Secrets**: The data directory `~/.omninet/` contains certificates, config, and database files. Do not commit these.
- **Passwords**: User passwords are hashed with bcrypt (`src/crypto/Password.cpp`). Raw tokens and API keys are returned only once at creation.
- **TLS defaults**: Client TLS contexts default to `SSL_VERIFY_PEER` with the system CA bundle (v0.7.2+).
- **CORS**: `*` is a special allow-all value. For credentialed requests, use explicit origins. The allowlist is parsed into a `std::unordered_set` for per-request lookup.
- **Rate limiting / intrusion detection**: Per-IP rate limiting and failed-attempt tracking are implemented in `src/dashboard/RateLimiter.cpp` and `src/intrusion/IntrusionDetector.cpp`.
- **Static file serving**: Be careful with catch-all routes (`/*`) when combined with the admin dashboard (`/omninet/*`). Register specific routes before catch-alls.
- **Buffer protection**: There is a dedicated `test_buffer_protection` executable for buffer-safety regression tests.

## 9. Development Conventions

### Versioning

- Version is detected from the latest git tag (`git describe --tags --abbrev=0`).
- Falls back to `0.1.0` if no tag exists.
- `v` prefix is stripped from tags.
- CMake defines `OMNINET_VERSION_MAJOR/MINOR/PATCH` as compile definitions on the `OmniNet` target and re-exports them through `omninet_tools_common`.

### Configuration and Data Directory

- Default data directory: `~/.omninet/`.
- Config file: `~/.omninet/config.json`.
- Logs: `~/.omninet/logs/omninet.log` (rotated).
- Database: `~/.omninet/dashboard.db`.
- Certificates: `~/.omninet/certs/`.
- Use `Config::instance()` to read/write global configuration.

### Routing

- Routing is **first-match-wins**.
- Register specific routes **before** wildcards (`/api/users/:id` before `/*`).
- Path parameters use `:name` and capture a single segment.
- Wildcards capture the remainder of the path.

### CLI Tools

Tools are built into `build/tools/`. Common workflow:

```bash
# Initialize data directory and admin user
./build/tools/omninet-init --yes

# Generate self-signed TLS certificates
./build/tools/omninet-certs generate

# View logs
./build/tools/omninet-log tail

# Manage users
./build/tools/omninet-users list
```

### Demos

Demos are built into `build/demos/`. Example:

```bash
./build/demos/http_server_demo      # http://localhost:8080
./build/demos/https_server_demo     # https://localhost:8443
```

## 10. Dependency Notes

- **OpenSSL**, **SQLite3**, and **pthread** are required system dependencies.
- **zlib** is optional; CMake detects it via `find_path`/`find_library`.
- **nlohmann/json** and **sqlite-vec** are downloaded automatically by CMake FetchContent on first configure. Ensure network access or pre-populate `build/_deps/`.
- `sqlite-vec` is built as a shared library `libvec0.so` in the binary directory and loaded at runtime via SQLite's extension mechanism. Its path is baked into `OmniNet` as `OMNINET_VEC0_PATH`.

## 11. Common Tasks

### Clean build

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Run a specific test executable

```bash
cmake --build build --target test_config
./build/tests/test_config
```

### Run static analysis

```bash
cmake --build build --target static_analysis
```

### Inspect current task

Read `prompt.txt` in the repository root. It contains the active feature request or bug report the project is working toward.

## 12. Gotchas

- `Server::port()` returns the configured port; `Server::boundPort()` returns the actual OS-assigned port (useful when port is `0`).
- The WebSocket upgrade path must use `registerProtocolSwitch()` to avoid a TOCTOU race; the legacy `setPostUpgradeHandler()` is deprecated.
- Tools use `tools_common` as a static library. When adding a new tool, register it in `tools/CMakeLists.txt` and add it to the `tools` custom target.
- Public compile definitions on the `OmniNet` static library do **not** transitively propagate through `tools_common`; `tools_common` re-exports `OMNINET_VERSION_*` explicitly.
- The project uses both a custom `Async::Task<T>` coroutine type and `std::expected`/`OmniNet::Result`; prefer `Result` for new error-handling APIs.
