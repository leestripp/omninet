# AGENTS.md — Guidance for AI Coding Agents

This file provides context and conventions for AI agents working on the OmniNet codebase.

## Project Overview

**OmniNet** is a modern, high-performance C++ HTTP/WebSocket server library. It ships as a
static library (`libOmniNet.a`) plus a suite of CLI admin tools, demos, benchmarks, and tests.

Major feature areas:

- **HTTP/1.1 server & client** — keep-alive, chunked transfer, routing with path params/wildcards, static file serving, CORS, optional gzip compression
- **WebSocket (RFC 6455)** — full client/server protocol support
- **TLS/HTTPS** — via OpenSSL (3.x recommended)
- **Session management & auth** — SQLite-backed session tokens, API keys with scopes, bcrypt password hashing
- **Admin dashboard** — web UI mounted at `/omninet`, with log viewer and user management
- **SSE (Server-Sent Events)** and `StreamingResponse` for real-time push
- **Game networking (UDP)** — reliable-ordered channels, piggyback ACKs, adaptive retransmission, delta snapshot compression, bit-stream serialization
- **DTLS** — encrypted UDP with stateless cookie exchange (HelloVerifyRequest), connection migration across NAT rebinds
- **Async stack** — C++20/23 coroutines (`Task<T>`, executors), epoll-based `IoContext`, `AsyncServer`, `AsyncDatabase`/`DatabasePool`, `AsyncLogger`, `AsyncTlsSocket`, `AsyncFile`
- **Vector store** — sqlite-vec extension for semantic/similarity search
- **Misc** — rate limiting, intrusion detection, log rotation, XDG base-directory compliance

Current version is derived from git tags (latest: **1.0.0+**); the fallback is `0.1.0`.
Version is baked into the library as `OMNINET_VERSION_MAJOR/MINOR/PATCH` compile definitions.

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | **C++23** (required), some C (sqlite-vec) |
| Build system | CMake 3.16+ |
| TLS/crypto | OpenSSL 1.1.1+ (3.x recommended) |
| Database | SQLite3 (+ sqlite-vec v0.1.6 extension, FetchContent) |
| JSON | nlohmann/json v3.11.3 (FetchContent, header-only) |
| Compression | zlib (optional, `OMNINET_HAVE_ZLIB` option) |
| Threading | pthreads, custom work-stealing `ThreadPool` |
| Async I/O | epoll (Linux), C++ coroutines |
| Tests | Custom header-only framework (`include/OmniNet/Test.h`) + CTest |
| Static analysis | cppcheck (`cmake --build build --target cppcheck`) |

**Compiler requirements:** GCC 13+ (preferred default), Clang 17+, or MSVC 19.36+.
The build defaults to `gcc/g++` when available; override with `CC`/`CXX`.

## Build & Test Commands

### Configure & build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

**Never configure in-source** (do not run `cmake .` in the repo root); always use `-S . -B build`.

### Key CMake options

| Option | Default | Purpose |
|--------|---------|---------|
| `OMNINET_BUILD_TESTS` | ON (standalone) | Build test executables |
| `OMNINET_BUILD_DEMOS` | ON (standalone) | Build demo apps |
| `OMNINET_BUILD_BENCHMARKS` | ON (standalone) | Build benchmarks |
| `OMNINET_BUILD_TOOLS` | ON (standalone) | Build CLI tools |
| `OMNINET_ENABLE_VEC` | ON | Build sqlite-vec vector extension |
| `OMNINET_HAVE_ZLIB` | ON | Enable gzip response compression |
| `BUILD_TESTING` | ON | Register tests with CTest |

All component options default to OFF when OmniNet is consumed via
`FetchContent`/`add_subdirectory` — only the static library is built then.

### Run tests

```bash
# All tests via CTest
cd build && ctest --output-on-failure

# Or run the comprehensive suite directly
./build/tests/omnet_test

# Or a single test binary, e.g.
./build/tests/test_config
```

Each test file is its own executable (`test_*`), all registered with CTest.
Networking tests bind to localhost ports — run them serially if you see flakiness
(`ctest -j1`).

### Static analysis

```bash
cmake --build build --target cppcheck   # fails only on error-level findings
```

### Run demos / tools

```bash
./build/demos/http_server_demo          # http://localhost:8080
./build/tools/omninet-init --yes        # first-time setup (config + admin user)
./build/tools/omninet-certs generate    # TLS certificates
```

## Project Structure

```
├── CMakeLists.txt         # Root build file (library, deps, options, cppcheck target)
├── include/OmniNet/       # Public headers (Test.h — user-facing test framework)
├── src/                   # Library source (include root: #include "http/Router.h")
│   ├── OmniNet.h/.cpp     # Umbrella header + init()/cleanup()
│   ├── config/            # Config singleton, JSON config, XDG paths
│   ├── utils/             # Logger, LogReader, Utils, Exception, Expected, XdgPaths
│   ├── crypto/            # Crypto helpers, bcrypt Password hashing
│   ├── net/               # Socket, TLSSocket, UdpSocket, DtlsSocket/Session, Buffer
│   ├── game/              # BitStream, GameProtocol/Connection/Server/Client, DeltaSnapshot
│   ├── http/              # Request/Response, Connection, Client, Router, Server,
│   │                      # StaticHandler, StreamingResponse
│   ├── websocket/         # Frame, Handshake, WebSocket
│   ├── dashboard/         # DashboardHandler, RateLimiter
│   ├── admin/             # AdminHandler (/omninet/* endpoints)
│   ├── db/                # Database (SQLite), SessionStore, UserStore,
│   │                      # AsyncDatabase, DatabasePool
│   ├── vector/            # VectorStore (sqlite-vec)
│   ├── token/             # SessionToken(+Store), ApiKey(+Store), TokenUtils
│   ├── thread/            # ThreadPool (work-stealing), Task, WorkStealingQueue
│   ├── async/             # Task<T>, Executor, Awaitable, AsyncHandler, AsyncLogger,
│   │                      # AsyncTlsSocket
│   ├── io/                # IoContext, EpollContext, AsyncSocket, AsyncFile
│   ├── server/            # AsyncServer (event-driven HTTP)
│   ├── sse/               # SSEHandler
│   └── intrusion/         # IntrusionDetector
├── tests/                 # One executable per test file; test_all.cpp = omnet_test
├── demos/                 # Example apps (http_server, https_server, websocket_echo, ...)
├── benchmarks/            # Performance benchmarks
├── tools/                 # CLI tools (omninet-init, -users, -certs, -log, -db, -ctl,
│                          # -mon, -status, -health, -firewall) + tools_common/ shared lib
├── static/                # Static assets for demos
├── docs/                  # docs/api/*.md references, docs/migration/* upgrade guides
├── resources/             # Banner/icon images
└── cmake/                 # Local CMake modules (FindCppcheck.cmake)
```

**Generated/ignored directories** (never commit, never edit): `build/`, `build-asan/`,
`build-tsan/`, `out/`, `third_party/` (FetchContent output), `test_data/`, `*.db`,
`.daedalus*/` (agent runtime state).

## Code Style & Conventions

- **Standard:** C++23, extensions OFF, `-Wall -Wextra -Wpedantic`. Warnings matter — keep
  builds warning-clean.
- **Namespaces:** everything lives under `OmniNet` with per-module sub-namespaces:
  `OmniNet::HTTP`, `OmniNet::WebSocket`, `OmniNet::DB`, `OmniNet::Token`, `OmniNet::Game`,
  `OmniNet::Async`, `OmniNet::IO`. Close with a comment: `} // namespace OmniNet::HTTP`.
- **Headers:** include guards of the form `OMNINET_<MODULE>_<NAME>_H` (some files also use
  `#pragma once`). Includes are relative to `src/`, e.g. `#include "net/Socket.h"` —
  never `../`-relative paths.
- **Naming:** `PascalCase` for classes/types, `camelCase` for methods/variables,
  `UPPER_SNAKE` for macros/constants. Structs named `Config` are the idiomatic way to
  parameterize components (e.g. `Server::Config`).
- **Formatting:** 4-space indentation, braces on the same line, generous section banners
  (`// ============...`) to delimit logical regions.
- **Error handling:** project-specific exception hierarchy (`utils/Exception.h`) and a
  `std::expected`-style type (`utils/Expected.h`) — prefer these over ad-hoc error codes
  in new code.
- **Changelog-in-code:** notable fixes carry version-tagged comments
  (e.g. `// v0.6.0 fix (bug #3): ...`). Follow this pattern when fixing bugs so the
  history is greppable.
- **Library lifecycle:** apps call `OmniNet::init()` before use and `OmniNet::cleanup()`
  at exit.
- **Singletons:** `Config::instance()` and `Logger::instance()` are the central config and
  logging services; use them rather than adding new globals.
- **Data locations:** follow the XDG Base Directory spec via `Config`/`XdgPaths`
  (`~/.config/omninet`, `~/.local/share/omninet`, `~/.local/state/omninet`, etc.). Do not
  hardcode `~/.omninet` paths in new code (legacy support already exists).

## Testing Conventions

- Use the project's own header-only framework: `#include <OmniNet/Test.h>` (public) or the
  local macro style seen in `tests/*.cpp` (`TEST(name)` / `RUN(name)` / `ASSERT_*`).
- **Adding a test** requires three edits in `tests/CMakeLists.txt`:
  1. `add_executable(test_<name> test_<name>.cpp)`
  2. `target_link_libraries(test_<name> PRIVATE OmniNet pthread)`
  3. `add_test(NAME test_<name> COMMAND test_<name>)`
- Prefer ephemeral ports/paths and clean up temp files; tests must not depend on each
  other or on execution order.

## Adding Code — Where Things Go

- **New library module:** headers + sources under `src/<module>/`, add `.cpp` files to
  `target_sources(OmniNet PRIVATE ...)` in the root `CMakeLists.txt`, and add includes to
  the umbrella header `src/OmniNet.h` if the API is public.
- **New CLI tool:** `tools/<name>/main.cpp` + `add_omninet_tool(<name>)` in
  `tools/CMakeLists.txt`; shared helpers belong in `tools/tools_common/`.
- **New demo/benchmark:** add source + a target in `demos/CMakeLists.txt` or
  `benchmarks/CMakeLists.txt`.
- **Docs:** update `README.md` for user-visible changes and add/extend a page under
  `docs/api/` for new public APIs; breaking changes need a note in `docs/migration/`.

## Gotchas

- **FetchContent needs network access** on first configure (nlohmann/json, sqlite-vec).
- **Version comes from git tags** — untagged checkouts report `0.1.0`.
- **Third-party warnings are suppressed** only for `vec0_extension`; don't weaken project
  warning flags to silence third-party noise.
- The `vec0` extension path is baked in as `OMNINET_VEC0_PATH` pointing into the build
  tree; tests using `VectorStore` need that build artifact present.
- Tools intentionally compile at C++20 (`cxx_std_20`) while the library requires C++23 —
  keep tool code free of C++23-only features.
- `CMakeFiles/` in the repo root is leftover from an accidental in-source configure —
  ignore it; never repeat the mistake.
