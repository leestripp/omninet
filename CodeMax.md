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
- [x] Add unit tests for utils

### Task 1.4: Crypto
- [x] Implement Crypto.h (OpenSSL wrapper)
- [x] Implement Crypto.cpp (SHA256, HMAC, PBKDF2, Base64)
- [x] Implement Password.cpp
- [x] Fix OpenSSL 3.x compatibility

### Task 1.5: Networking
- [x] Implement Socket.h (TCP wrapper)
- [x] Implement Socket.cpp
- [x] Implement TLSSocket.h
- [x] Implement TLSSocket.cpp

### Task 1.6: HTTP Protocol
- [x] Implement HttpRequest.h/HttpRequest.cpp
- [x] Implement HttpResponse.h/HttpResponse.cpp
- [x] Implement HttpConnection.h/HttpConnection.cpp

### Task 1.7: HTTP Server
- [x] Implement Router.h/Router.cpp
- [x] Implement HttpServer.h/HttpServer.cpp
- [x] Implement StaticHandler.h/StaticHandler.cpp

### Task 1.8: WebSocket
- [x] Implement WebSocketFrame.h
- [x] Implement WebSocketHandshake.h
- [x] Implement WebSocket.h

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

### Task 1.13: Fix All Compilation Issues
- [x] Fix Database.h include path (`../../utils/Exception.h` → `utils/Exception.h`)
- [x] Add missing `#include <mutex>` to StaticHandler.h for `std::once_flag`
- [x] Add HTTP forward declarations to RateLimiter.h/DashboardHandler.h
- [x] Fix TLSSocket.h method declarations (removed invalid `accept()` override)
- [x] Fix TLSSocket.h - added includes for Exception.h and Crypto.h
- [x] Fix Crypto.cpp for OpenSSL 3.x (removed deprecated EVP_KDF APIs, fixed HKDF implementation)
- [x] Fix WebSocketFrame.cpp - CloseCode bitwise AND issue
- [x] Fix Socket.h - added include for Exception.h
- [x] Fix HttpRequest.cpp - parseQueryString() const issue
- [x] Fix HttpConnection.cpp - Handler/ErrorHandler qualified names
- [x] Fix WebSocket.h - added condition_variable, HTTP includes
- [x] Fix WebSocket.cpp - ambiguous nullptr constructor
- [x] Build succeeds with all modules
- [x] Tests pass (100% pass rate)

---

## Implementation Status

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
| 5 | Database | ⬜ Pending |
| 6 | Dashboard | ⬜ Pending |
| 7 | SSE | ⬜ Pending |
| 8 | Intrusion Detection | ⬜ Pending |
| 9 | Testing | ⬜ Pending |
| 10 | Demos | ⬜ Pending |

---

## Build Status: ✅ ALL MODULES COMPILE SUCCESSFULLY

- Library: `libOmniNet.a` compiled successfully
- All modules included: OmniNet, Config, Logger, Exception, Utils, Crypto, Socket, TLSSocket, HTTP, WebSocket
- All tests pass (100% pass rate)

---

## Notes

- Build command: `cmake -S . -B build && cmake --build build -j$(nproc)`
- Static library output: `build/lib/libOmniNet.a`
- Tests output: `build/tests/`
- Git branch: `main`
