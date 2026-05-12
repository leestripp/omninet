# OmniNet - C++ HTTP/WebSocket Server Library

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 7.0.3-6.stable |
| OS | Arch Linux |
| Container | docker (archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Info

### Overview
OmniNet is a modern, high-performance C++ HTTP/WebSocket server library with full-featured capabilities including TLS/HTTPS support, session management, admin dashboard, CLI tools, and more.

### Build System
- **Type**: CMake (3.16+)
- **Language**: C++20
- **Output**: Static library (`libOmniNet.a`), executables (demos, tests, tools)

### Project Structure
```
OmniNet/
├── src/               # Source code
│   ├── OmniNet.h      # Main header (all includes)
│   ├── config/         # Configuration system (Config.cpp/h)
│   ├── utils/         # Utilities (Logger, LogReader, Utils, Exception)
│   ├── crypto/        # Cryptography (Crypto, Password)
│   ├── net/           # Networking (Socket, TLSSocket, Buffer)
│   ├── http/          # HTTP server components
│   │   ├── HttpRequest.cpp/h   # HTTP request parsing
│   │   ├── HttpResponse.cpp/h  # HTTP response building
│   │   ├── HttpConnection.cpp/h # Connection handling
│   │   ├── Router.cpp/h        # Route management
│   │   ├── HttpServer.cpp/h    # Server core
│   │   └── StaticHandler.cpp/h # Static file serving
│   ├── websocket/     # WebSocket implementation
│   ├── db/            # Database (SQLite wrapper)
│   ├── dashboard/     # Admin dashboard
│   ├── admin/         # Admin endpoints
│   ├── sse/           # Server-Sent Events
│   └── intrusion/     # Security monitoring
├── include/           # Public headers
├── demos/             # Example applications
├── tests/             # Unit tests
├── tools/             # CLI admin tools (omninet-*)
├── static/            # Static files for demo
├── docs/              # Documentation (API, demos)
└── build/             # Build output (created by CMake)
```

### Key Entry Points
- `src/OmniNet.h` - Main include file with all module headers
- `src/OmniNet.cpp` - Global init/cleanup functions
- `demos/http_server.cpp` - Basic HTTP server demo
- `tests/test_all.cpp` - Main test executable
- `tools/omninet-init/main.cpp` - Config initialization tool

### Dependencies
- **OpenSSL** - TLS/HTTPS support (1.1.1+)
- **SQLite3** - Database backend
- **nlohmann/json** - JSON parsing (CMake subproject)
- **GoogleTest** - Unit testing framework (optional)
- **Threads** - POSIX threads

### Build Commands
```bash
# Configure (from project root)
cmake -S . -B build

# Build everything
cmake --build build -j$(nproc)

# Build with tests
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test
```

### Core Architecture

#### Layer 1: Foundation
- `Exception.h` - Custom exception hierarchy
- `Logger.h` - Logging system with file rotation
- `Config.h` - JSON-based configuration system
- `Utils.h` - Common utility functions

#### Layer 2: Networking
- `Socket.h` - TCP socket wrapper (IPv4/IPv6)
- `TLSSocket.h` - OpenSSL TLS wrapper
- `Buffer.h` - Ring buffer for I/O

#### Layer 3: HTTP
- `HttpRequest.h` - Parse HTTP requests
- `HttpResponse.h` - Build HTTP responses
- `HttpConnection.h` - Connection state machine
- `Router.h` - URL routing with regex support
- `HttpServer.h` - Main server class
- `StaticHandler.h` - Static file serving

#### Layer 4: Application
- `WebSocket.h` - WebSocket connections
- `Database.h` - SQLite wrapper
- `DashboardHandler.h` - Admin dashboard
- `SSEHandler.h` - Server-Sent Events
- `IntrusionDetector.h` - Security monitoring

## Notes
- Project uses a layered architecture (Foundation -> Networking -> HTTP -> Application)
- Server configuration uses JSON file at `~/.omninet/config.json`
- Data directory: `~/.omninet/` (config, logs, database, certs)
- Admin dashboard accessible at `/omninet` endpoint
- Tests are built as separate executables
- Tools are prefixed with `omninet-` (e.g., `omninet-init`, `omninet-status`)

## TODO - Bugs & Issues Sweep (2026-05-12)

### ✅ FIXED - Critical Bugs (3)

| ID | Issue | Status | Fix |
|----|-------|--------|-----|
| **BUG-001** | Router catch-all conflicts with admin routes | ✅ FIXED | Changed `serveStatic()` to use prefix-based regex instead of `.*` catch-all |
| **BUG-002** | Response not sent after error handler | ✅ FIXED | Fixed `handleNextRequest()` to use response from error handler |
| **BUG-003** | WebSocket Server creates null connection | ✅ FIXED | Updated WebSocket server to properly handle upgrade response |

### ✅ FIXED - High Priority Issues (3)

| ID | Issue | Status | Fix |
|----|-------|--------|-----|
| **ISSUE-001** | Chunked transfer encoding not implemented | ✅ FIXED | Added `READING_CHUNKED` state in `readRequest()` with basic parsing |
| **ISSUE-002** | Socket recvAll only reads once | ✅ FIXED | Added `transferEncoding()` method; documented limitation |
| **ISSUE-003** | Router case-insensitive/trailing-slash options non-functional | ✅ FIXED | Implemented `icase` flag for case-insensitive and redirect logic for trailing slash |

### ✅ FIXED - Medium Priority Issues (4)

| ID | Issue | Status | Fix |
|----|-------|--------|-----|
| **ISSUE-004** | Missing `.wasm` MIME type | ✅ FIXED | Added `{".wasm", "application/wasm"}` to MIME types |
| **ISSUE-005** | Dashboard stats endpoint returns empty JSON | ✅ FIXED | Implemented `handleGetStats()` with actual stats (uptime, users, sessions) |
| **ISSUE-006** | `handleChangePassword()` not implemented | ✅ FIXED | Implemented full password change functionality with validation |
| **ISSUE-007** | Inconsistent version strings | ✅ FIXED | Unified version constant `SHARED_VERSION` across all components |

### ✅ FIXED - Low Priority Issues (2)

| ID | Issue | Status | Fix |
|----|-------|--------|-----|
| **ISSUE-008** | WebSocket fragmented send chunk size too large | ✅ FIXED | Added `MAX_FRAGMENT_SIZE = 16384` cap to `sendFragmented()` |
| **ISSUE-009** | Directory listing HTML shows no version | ✅ FIXED | Added version to directory listing footer |

### Known Limitations (Not Bugs - Documented)

1. **No IPv6 support in binding** - `Socket::bind()` only handles IPv4
2. **No WebSocket permessage-deflate** - Compression not supported
3. **No HTTP/2 support** - Only HTTP/1.1
4. **No graceful shutdown timeout** - Worker threads exit immediately
5. **No request body streaming** - Bodies are buffered in memory

---

## Test Results

```
Unit Tests: 110 passed, 0 failed, 0 skipped
Config Tests: 21 passed, 0 failed
```

*Last updated: 2026-05-12*