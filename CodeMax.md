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
│   ├── config/
│   │   ├── Config.h           # Configuration singleton
│   │   └── Config.cpp
│   ├── utils/
│   │   ├── Exception.h        # Base exception class
│   │   ├── Logger.h           # Logging with levels
│   │   ├── Logger.cpp
│   │   ├── Utils.h            # String, encoding helpers
│   │   └── Utils.cpp
│   ├── crypto/
│   │   ├── Crypto.h           # OpenSSL wrapper (TLS init, etc.)
│   │   ├── Crypto.cpp
│   │   └── Password.cpp       # Password hashing
│   ├── net/
│   │   ├── Socket.h           # Base socket wrapper
│   │   ├── Socket.cpp
│   │   └── TLSSocket.h        # OpenSSL TLS socket
│   ├── http/
│   │   ├── HttpRequest.h
│   │   ├── HttpResponse.h
│   │   ├── HttpConnection.h
│   │   ├── Router.h
│   │   ├── HttpServer.h
│   │   └── StaticHandler.h
│   ├── websocket/
│   │   ├── WebSocketFrame.h
│   │   ├── WebSocketHandshake.h
│   │   └── WebSocket.h
│   ├── db/
│   │   ├── Database.h
│   │   ├── SessionStore.h
│   │   └── UserStore.h
│   ├── dashboard/
│   │   ├── DashboardHandler.h
│   │   └── RateLimiter.h
│   ├── sse/
│   │   └── SSEHandler.h
│   └── intrusion/
│       └── IntrusionDetector.h
├── tests/
└── demos/
```

### Key Entry Points
- Main include: `src/OmniNet.h`
- CMakeLists.txt (root)

---

## Current Phase: Phase 1 - Foundation (Core Infrastructure)

### Tasks
1. **Build System Setup** ✅
   - Create CMakeLists.txt with C++20
   - Configure findOpenSSL, findSQLite3
   - Define OMNINET_VERSION macro
   - Setup include directories

2. **Exception & Logging** ✅
   - Exception hierarchy (IOException, SocketException, TLSException, ParseException, DatabaseException)
   - Logger singleton with file output
   - Timestamps, thread ID to log entries

3. **Utils** ✅
   - String utilities (split, trim, join)
   - URL encoding/decoding (percent-encoding)
   - Base64 encode/decode
   - Hex conversion utilities

---

## Todos

### Task 1.1: Build System
- [x] Create CMakeLists.txt with static library target
- [x] Create directory structure
- [x] Configure C++20, find OpenSSL, find SQLite3
- [x] Add version macro (OMNINET_VERSION)

### Task 1.2: Exception & Logging
- [x] Implement Exception.h with hierarchy
- [x] Implement Logger.h singleton
- [x] Implement Logger.cpp with file output
- [x] Add timestamps and thread ID formatting
- [x] Create main OmniNet.h include

### Task 1.3: Utils
- [x] Implement Utils.h (declarations)
- [x] Implement Utils.cpp (string, URL, base64, hex)
- [ ] Add unit tests for utils

### Task 1.4: Crypto
- [x] Implement Crypto.h (OpenSSL wrapper)
- [x] Implement Crypto.cpp (SHA256, HMAC, PBKDF2, Base64)
- [x] Implement Password.cpp

### Task 1.5: Networking
- [x] Implement Socket.h (TCP wrapper)
- [x] Implement Socket.cpp
- [ ] Implement TLSSocket.h

### Task 1.6: HTTP Protocol
- [ ] Implement HttpRequest.h/HttpRequest.cpp
- [ ] Implement HttpResponse.h/HttpResponse.cpp
- [ ] Implement HttpConnection.h/HttpConnection.cpp

### Task 1.7: HTTP Server
- [ ] Implement Router.h/Router.cpp
- [ ] Implement HttpServer.h/HttpServer.cpp
- [ ] Implement StaticHandler.h/StaticHandler.cpp

### Task 1.8: WebSocket
- [ ] Implement WebSocketFrame.h
- [ ] Implement WebSocketHandshake.h
- [ ] Implement WebSocket.h

### Task 1.9: Database
- [ ] Implement Database.h/Database.cpp
- [ ] Implement SessionStore.h
- [ ] Implement UserStore.h

### Task 1.10: Dashboard
- [ ] Implement DashboardHandler.h
- [ ] Implement RateLimiter.h

### Task 1.11: SSE
- [ ] Implement SSEHandler.h

### Task 1.12: Intrusion Detection
- [ ] Implement IntrusionDetector.h

---

## Implementation Status

| Phase | Component | Status |
|-------|-----------|--------|
| 1.1 | Build System | ✅ Complete |
| 1.2 | Exception & Logging | ✅ Complete |
| 1.3 | Utils | ✅ Complete |
| 1.4 | Crypto | ✅ Complete |
| 1.5 | Networking (Socket) | ✅ Complete |
| 1.6 | Networking (TLS) | 🔄 In Progress |
| 2 | HTTP Protocol | 🔄 In Progress |
| 3 | HTTP Server | ⬜ Pending |
| 4 | WebSocket | ⬜ Pending |
| 5 | Database | ⬜ Pending |
| 6 | Dashboard | ⬜ Pending |
| 7 | SSE | ⬜ Pending |
| 8 | Intrusion Detection | ⬜ Pending |
| 9 | Testing | ⬜ Pending |
| 10 | Demos | ⬜ Pending |

---

## Notes

- Build command: `cmake -S . -B build && cmake --build build -j$(nproc)`
- Static library output: `build/lib/libOmniNet.a`
- Tests output: `build/tests/`
- Git branch: `main` (initial commit done)
