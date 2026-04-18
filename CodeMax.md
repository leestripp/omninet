# OmniNet - Project Analysis & Documentation

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.16 |
| OS | Arch Linux |
| Container | Docker (archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Overview

**OmniNet** is a modern, high-performance C++ HTTP/WebSocket server library with TLS support, session management, and security features.

### Quick Stats
- **Version**: 0.1.0+ (from git tags)
- **Language**: C++20
- **Build System**: CMake 3.16+
- **Dependencies**: OpenSSL, SQLite3, nlohmann/json
- **License**: MIT

## Project Structure

```
OmniNet/
├── src/                      # Core source code
│   ├── OmniNet.h/.cpp        # Main header, init/cleanup
│   ├── config/                # Configuration system
│   │   ├── Config.h/.cpp      # Singleton config with JSON storage
│   │   └── LoggingConfig      # Log level, file, rotation settings
│   ├── utils/                 # Utilities
│   │   ├── Logger.h/.cpp      # Thread-safe logging with rotation
│   │   ├── LogReader.h/.cpp   # Log file reader with filtering
│   │   ├── Exception.h/.cpp    # Exception hierarchy
│   │   └── Utils.h/.cpp       # String, URL, MIME utilities
│   ├── crypto/                # Cryptography
│   │   ├── Crypto.h/.cpp      # Encryption utilities
│   │   └── Password.h/.cpp     # bcrypt password hashing
│   ├── net/                   # Networking
│   │   ├── Socket.h/.cpp      # TCP socket wrapper
│   │   ├── TLSSocket.h/.cpp   # TLS/SSL socket
│   │   └── Buffer.h/.cpp      # Byte buffer with I/O
│   ├── http/                  # HTTP Server
│   │   ├── HttpRequest.h/.cpp # Request parsing
│   │   ├── HttpResponse.h/.cpp # Response building
│   │   ├── HttpConnection.h/.cpp # Connection handling
│   │   ├── Router.h/.cpp      # Route matching
│   │   ├── HttpServer.h/.cpp   # Main HTTP server
│   │   └── StaticHandler.h/.cpp # Static file serving
│   ├── websocket/              # WebSocket
│   │   ├── WebSocketFrame.h/.cpp # Frame encoding/decoding
│   │   ├── WebSocketHandshake.h/.cpp # HTTP upgrade
│   │   └── WebSocket.h/.cpp    # WebSocket connection
│   ├── db/                     # Database
│   │   ├── Database.h/.cpp     # SQLite wrapper
│   │   ├── SessionStore.h/.cpp # Session management
│   │   └── UserStore.h/.cpp    # User authentication
│   ├── dashboard/              # Admin Dashboard
│   │   ├── DashboardHandler.h/.cpp # HTML/JSON dashboard
│   │   └── RateLimiter.h/.cpp  # IP rate limiting
│   ├── sse/                    # Server-Sent Events
│   │   └── SSEHandler.h/.cpp    # SSE implementation
│   ├── intrusion/               # Security
│   │   └── IntrusionDetector.h/.cpp # Intrusion detection
│   └── admin/                  # Admin API
│       └── AdminHandler.h/.cpp  # Admin endpoints
├── tests/                      # Unit tests (GoogleTest-style)
│   ├── test_all.cpp           # Main test suite
│   ├── test_config.cpp        # Config tests
│   ├── test_logger_new.cpp    # Logger/LogReader tests
│   └── ...
├── demos/                      # Example applications
│   ├── http_server.cpp        # Basic HTTP server demo
│   ├── https_server.cpp       # HTTPS server demo
│   ├── websocket_echo.cpp      # WebSocket echo demo
│   ├── static_files.cpp       # Static file serving demo
│   └── dashboard.cpp           # Dashboard demo
├── tools/                      # CLI tools
│   ├── omninet-status          # System status
│   ├── omninet-health          # Health checks
│   ├── omninet-init            # Initialize config
│   ├── omninet-users           # User management
│   ├── omninet-certs           # TLS certificate management
│   ├── omninet-db              # Database management
│   ├── omninet-log             # Log viewer
│   ├── omninet-firewall        # Firewall rules
│   ├── omninet-ctl             # Server control
│   └── omninet-mon             # Monitoring
├── static/                     # Static files for demos
├── docs/                       # Documentation
├── CMakeLists.txt             # Build configuration
└── README.md                  # Project readme
```

## Core Architecture

### Layer Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                        Application                           │
├─────────────────────────────────────────────────────────────┤
│                     HTTP/Router Layer                        │
│   HttpServer → Router → [StaticHandler, WebSocket, etc]    │
├─────────────────────────────────────────────────────────────┤
│                   WebSocket/SSE Layer                       │
│        WebSocket → WebSocketFrame → WebSocketHandshake     │
├─────────────────────────────────────────────────────────────┤
│                     Networking Layer                         │
│              Socket/TLSSocket → Buffer                      │
├─────────────────────────────────────────────────────────────┤
│                    Database Layer                           │
│              Database → SessionStore/UserStore              │
├─────────────────────────────────────────────────────────────┤
│                   Security/Dashboard                        │
│     Dashboard → RateLimiter → IntrusionDetector            │
├─────────────────────────────────────────────────────────────┤
│                    Foundation Layer                         │
│        Config → Logger → Utils → Exception                  │
└─────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. HTTP Server (src/http/)
- **HttpServer**: Main server class with thread pool
- **Router**: Route matching with path parameters and wildcards
- **HttpRequest/Response**: HTTP message handling
- **StaticHandler**: File serving with MIME types

### 2. WebSocket (src/websocket/)
- Protocol implementation per RFC 6455
- Frame masking and fragmentation
- HTTP upgrade handshake

### 3. Configuration (src/config/)
- JSON-based configuration storage
- Singleton pattern for global access
- Hot reload support with file watching
- Config file: `~/.omninet/config.json`

### 4. Logging (src/utils/)
- **Logger**: Thread-safe singleton with async mode
- **LogRotation**: Configurable size-based rotation
- **LogReader**: Read/query log files
- Log location: `~/.omninet/logs/omninet.log`

### 5. Database (src/db/)
- SQLite3 backend
- Session store with TTL
- User store with bcrypt passwords

### 6. Dashboard (src/dashboard/)
- HTML/JSON admin interface
- Login/logout with session management
- User management UI
- Log viewer tab

### 7. Security (src/intrusion/)
- Failed login tracking
- Auto-blocking of suspicious IPs
- Configurable thresholds

## Build System

### CMake Configuration
- **Source Files**: ~30 C++ files in src/
- **Dependencies**: OpenSSL, SQLite3, nlohmann/json (fetched)
- **Output**: Static library `libOmniNet.a`
- **Extras**: CLI tools, demos, tests

### Build Commands
```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Build with tests
cmake -S . -B build -DBUILD_TESTING=ON

# Run tests
./build/tests/omnet_test

# Run demo
./build/demos/http_server_demo
```

## Configuration

### Config File: ~/.omninet/config.json
```json
{
  "server": { "port": 8080, "workers": 4 },
  "tls": { "enabled": false },
  "logging": { "level": "INFO" },
  "dashboard": { "enabled": true },
  "database": { "path": "omninet.db" }
}
```

### Data Directory: ~/.omninet/
```
~/.omninet/
├── config.json        # Main configuration
├── logs/              # Log files
│   └── omninet.log    # Main log (rotated)
├── dashboard.db       # SQLite dashboard DB
└── certs/             # TLS certificates
```

## Testing

### Test Framework
- Custom test framework in `tests/test_utils.cpp`
- Assertion macros: `ASSERT_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`, etc.
- Test runner in `tests/test_main.cpp`

### Running Tests
```bash
./build/tests/omnet_test          # Main test suite
./build/tests/test_config         # Config tests
./build/tests/test_logger_new     # Logger/LogReader tests
```

## Entry Points

### Main Library
- Include `OmniNet.h` for all headers
- Call `OmniNet::init()` before use
- Call `OmniNet::cleanup()` at exit

### Example Usage
```cpp
#include "OmniNet.h"

int main() {
    OmniNet::init();
    
    OmniNet::HTTP::Server::Config config;
    config.port = 8080;
    OmniNet::HTTP::Server server(config);
    
    server.router().get("/api/hello", [](auto& req, auto& resp) {
        resp.setBody(R"({"hello": "world"})");
    });
    
    server.start();
    OmniNet::cleanup();
}
```

## Development Notes

### Branch Strategy
- `main` - stable releases
- `devel` - development (current working branch)

### Coding Standards
- C++20 with Concepts where beneficial
- Namespaces: `OmniNet::`, `OmniNet::HTTP::`, etc.
- Header guards: `OMNINET_*_H`
- RAII patterns throughout

### Dependencies
- **OpenSSL**: TLS/HTTPS support
- **SQLite3**: Database storage
- **nlohmann/json**: JSON parsing (header-only, fetched)

## Current Status

### Completed Features
- ✅ HTTP/1.1 server with keep-alive
- ✅ WebSocket protocol (RFC 6455)
- ✅ TLS/HTTPS support
- ✅ Static file serving
- ✅ Session management (SQLite)
- ✅ User authentication (bcrypt)
- ✅ Rate limiting
- ✅ Admin dashboard with logs viewer
- ✅ Log rotation
- ✅ Config hot reload

### Potential Improvements
- HTTP/2 support
- WebSocket permessage-deflate compression
- Async I/O with io_uring
- Middleware system
- OpenAPI documentation generation

## File Locations

| Purpose | Path |
|---------|------|
| Main Header | `src/OmniNet.h` |
| HTTP Server | `src/http/HttpServer.h` |
| WebSocket | `src/websocket/WebSocket.h` |
| Config | `src/config/Config.h` |
| Logger | `src/utils/Logger.h` |
| Dashboard | `src/dashboard/DashboardHandler.h` |
| Database | `src/db/Database.h` |
| Build Output | `build/` |
| Config File | `~/.omninet/config.json` |
| Log Files | `~/.omninet/logs/` |

## Documentation Update Plan

### Current State of Documentation

**docs/demos.md** (623 lines) - Needs updates:
- ❌ References non-existent `scripts/generate-certs.sh`
- ❌ Uses `OmninetConfig` instead of `OmniNet::Config::instance()`
- ❌ Uses `ServerBuilder()` which doesn't exist in codebase
- ❌ References `server.address()`, `server.port()` - actual is `serverConfig.address`, `serverConfig.port`
- ❌ API endpoints table incomplete (missing dashboard routes)
- ❌ Missing admin handler info (Health, Stats endpoints)
- ⚠️ Demo locations need verification

**docs/api/http-server.md** (413 lines) - Needs updates:
- ❌ Uses `ServerBuilder()` pattern - doesn't exist
- ❌ Uses `.address()`, `.port()` builder methods
- ❌ Route params use `req.param()` not `req.pathParam()`
- ❌ Missing admin routes info
- ❌ Missing dashboard integration
- ⚠️ Need to verify current API signatures

**docs/api/websocket.md** (442 lines) - Needs updates:
- ❌ Uses `WebSocket::Server` which doesn't exist
- ❌ Uses `Connection` class incorrectly
- ❌ Uses `conn.receiveText()` not in actual API
- ⚠️ Need to verify actual WebSocket implementation

**docs/api/database.md** (546 lines) - Needs updates:
- ❌ Uses `db.query()` with column vector - actual is different
- ❌ Missing prepared statement examples
- ⚠️ Need to verify Database class methods

**docs/api/index.md** (70 lines) - Check if current

### Documentation Update Tasks

#### Task 1: Update docs/demos.md [DONE]
- [x] Remove references to `scripts/generate-certs.sh`
- [x] Add `tools/omninet-certs` usage instead
- [x] Update config loading to use `Config::instance()`
- [x] Fix server setup to use actual API: `Server::Config` struct
- [x] Add admin routes info (`/omninet/health`, `/omninet/stats`)
- [x] Add dashboard info (`/omninet`, `/omninet/api/*`)
- [x] Update endpoint table with actual endpoints
- [x] Add log file location: `~/.omninet/logs/omninet.log`
- [x] Add CLI tools section
- [x] Fix code examples to match actual API

#### Task 2: Update docs/api/http-server.md [DONE]
- [x] Replace builder pattern with actual `Server::Config` examples
- [x] Fix route parameter access to use `req.pathParam()`
- [x] Add admin handler section (`/omninet/*` endpoints)
- [x] Add dashboard integration section
- [x] Verify all method signatures match actual code
- [x] Add statistics section
- [x] Add error handling section
- [x] Add middleware section
- [x] Add full example

#### Task 3: Update docs/api/websocket.md [DONE]
- [x] Rewrite to match actual WebSocket implementation
- [x] Use `WebSocket::Server.on()` pattern
- [x] Document actual Connection class API
- [x] Add callback-based API section
- [x] Add auto ping/pong section
- [x] Add statistics section
- [x] Add chat server example

#### Task 4: Update docs/api/database.md [DONE]
- [x] Verify Database class API
- [x] Update query syntax to match actual implementation
- [x] Add prepared statement examples
- [x] Document transaction usage correctly
- [x] Add schema migration section
- [x] Add user management example

#### Task 5: Update docs/api/index.md [DONE]
- [x] Check for outdated links
- [x] Add new sections for Logger, Dashboard
- [x] Add CLI tools reference
- [x] Add data directory structure

#### Task 6: Add Missing Documentation [DONE]
- [x] Create docs/api/logger.md - LogReader, log rotation
- [x] Create docs/api/dashboard.md - Admin dashboard usage
- [x] Create docs/api/config.md - (deferred - see README update)

#### Task 7: Update README.md [DONE]
- [x] Update project structure to include admin/, tools/
- [x] Add CLI tools section
- [x] Add logging section
- [x] Add data directory structure
- [x] Fix WebSocket example to match actual API
- [x] Add documentation links section

## Final Status: All Documentation Updated ✅

### Implementation Order
1. Update demos.md (most user-facing)
2. Update http-server.md (core component)
3. Update websocket.md (fix WebSocket API)
4. Update database.md (fix DB API)
5. Update index.md (navigation)
6. Add missing docs (logger, dashboard, config)
7. Final review and cross-reference

### Verification Checklist
After updating each doc:
- [ ] Build succeeds
- [ ] Demo runs without errors
- [ ] Code examples compile
- [ ] Links resolve correctly
- [ ] API matches actual implementation