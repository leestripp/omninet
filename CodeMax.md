# OmniNet - CodeMax Planning

## System Information
<!-- system_info output -->
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.16-133.desktop |
| OS | Arch Linux |
| Container | podman (archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Info
- **Project**: OmniNet - C++ HTTP/WebSocket Server Library
- **Language**: C++20
- **Build System**: CMake
- **Target**: Static library (libOmniNet.a)

### Dependencies
- OpenSSL (TLS/SSL and crypto)
- SQLite3 (data persistence)
- POSIX sockets (system)

### Directory Structure
```
OmniNet/
├── CMakeLists.txt
├── src/
│   ├── OmniNet.h              # Main include
│   ├── OmniNet.cpp            # Version info
│   ├── config/                # Configuration singleton
│   ├── utils/                 # Utilities (Exception, Logger, Utils)
│   ├── crypto/                # OpenSSL wrapper (TLS, crypto)
│   ├── net/                   # Networking (Socket, TLSSocket)
│   ├── http/                  # HTTP (Request, Response, Server, Router)
│   ├── websocket/              # WebSocket support
│   ├── db/                    # Database, Session, User stores
│   ├── dashboard/              # Admin dashboard handler
│   ├── sse/                   # Server-Sent Events handler
│   └── intrusion/              # Intrusion detection
├── tests/                     # Unit tests
└── demos/                     # Demo applications
```

---

## Implementation Status: ✅ ALL MODULES COMPLETE

| Phase | Component | Status |
|-------|-----------|--------|
| 1.1 | Build System | ✅ Complete |
| 1.2 | Exception & Logging | ✅ Complete |
| 1.3 | Utils | ✅ Complete |
| 1.4 | Crypto | ✅ Complete |
| 1.5 | Networking (Socket) | ✅ Complete |
| 1.6 | Networking (TLS) | ✅ Complete |
| 2 | HTTP Protocol | ✅ Complete |
| 3 | HTTP Server | ✅ Complete |
| 4 | WebSocket | ✅ Complete |
| 5 | Database | ✅ Complete |
| 6 | Dashboard | ✅ Complete |
| 7 | SSE | ✅ Complete |
| 8 | Intrusion Detection | ✅ Complete |

---

## Completed Modules Summary

### Core Foundation
- **Build System**: CMake with C++20, OpenSSL, SQLite3
- **Exception & Logging**: Full exception hierarchy, thread-safe logging
- **Utils**: String utilities, URL encoding, Base64, HMAC

### Networking
- **Socket**: TCP socket wrapper with IPv4/IPv6 support
- **TLSSocket**: Full TLS/SSL support with OpenSSL (client/server modes, SNI, certificates)

### HTTP
- **HttpRequest**: Request parsing, query params, headers, body
- **HttpResponse**: Response building, status codes, JSON support
- **HttpConnection**: Connection handling, keep-alive, timeouts
- **HttpServer**: Full HTTP server with routing
- **Router**: Path matching, middleware support
- **StaticHandler**: Static file serving

### WebSocket
- **WebSocketFrame**: Frame encoding/decoding (RFC 6455)
- **WebSocketHandshake**: Hybi handshake handling
- **WebSocket**: Full-duplex communication, ping/pong, extensions

### Database
- **Database**: SQLite wrapper with transactions, prepared statements
- **SessionStore**: Session management with TTL, cleanup
- **UserStore**: User authentication, password hashing, RBAC

### Dashboard
- **DashboardHandler**: Admin UI, login/logout, user management
- **RateLimiter**: IP-based rate limiting

### Real-time
- **SSEHandler**: Server-Sent Events support

### Security
- **IntrusionDetector**: SQL injection, XSS, path traversal detection
- **IPAllowlist**: IP/CIDR allowlisting

---

## Build Status: ✅ ALL TESTS PASS

- Library: `libOmniNet.a` compiled successfully
- All modules included in library
- All tests pass (100% pass rate)

---

## Build & Test Commands

```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test
```

## Outputs
- Static library: `build/lib/libOmniNet.a`
- Tests: `build/tests/omnet_test`
- Git branch: `main`

---

## Notes
- C++20 with modules support ready
- OpenSSL 3.x compatible
- Thread-safe throughout
- Comprehensive error handling
- Ready for production use
