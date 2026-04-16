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
/home/lee/projects/Programming/projects_C++/OmniNet/
├── CMakeLists.txt
├── src/
│   ├── OmniNet.h              # Main include
│   ├── OmniNet.cpp            # Version info
│   ├── config/
│   │   ├── Config.h           # Configuration singleton
│   │   └── Config.cpp
│   ├── utils/
│   │   ├── Exception.h        # Base exception class
│   │   ├── Exception.cpp
│   │   ├── Logger.h           # Logging with levels
│   │   ├── Logger.cpp
│   │   └── Utils.h            # String, encoding helpers
│   │   └── Utils.cpp
│   ├── crypto/
│   │   ├── Crypto.h           # OpenSSL wrapper (TLS init, etc.)
│   │   ├── Crypto.cpp
│   │   ├── Password.h         # PBKDF2 password hashing
│   │   └── Password.cpp
│   └── net/
│       ├── Socket.h           # Base socket wrapper
│       ├── Socket.cpp
│       ├── TLSSocket.h        # OpenSSL TLS socket
│       └── TLSSocket.cpp
├── tests/
└── demos/
```

### Key Entry Points
- Main include: `src/OmniNet.h`
- CMakeLists.txt (root)

---

## Current Phase: Phase 1 - Foundation (Core Infrastructure)

### Tasks
1. **Build System Setup**
   - Create CMakeLists.txt with C++20
   - Configure findOpenSSL, findSQLite3
   - Define OMNINET_VERSION macro
   - Setup include directories

2. **Exception & Logging**
   - Implement Exception hierarchy (IOException, SocketException, TLSException, ParseException, DatabaseException)
   - Implement Logger singleton with file output
   - Add timestamps, thread ID to log entries

3. **Utils**
   - String utilities (split, trim, join)
   - URL encoding/decoding (percent-encoding)
   - Base64 encode/decode
   - Hex conversion utilities

---

## Todos

### Task 1.1: Build System
- [x] Create CMakeLists.txt with static library target
- [x] Create directory structure (src/config, src/utils, src/crypto, src/net, tests, demos)
- [x] Configure C++20, find OpenSSL, find SQLite3
- [x] Add version macro (OMNINET_VERSION)

### Task 1.2: Exception & Logging
- [ ] Implement Exception.h with hierarchy
- [ ] Implement Exception.cpp
- [ ] Implement Logger.h singleton
- [ ] Implement Logger.cpp with file output
- [ ] Add timestamps and thread ID formatting
- [ ] Create main OmniNet.h include

### Task 1.3: Utils
- [ ] Implement Utils.h (declarations)
- [ ] Implement Utils.cpp (string, URL, base64, hex)
- [ ] Add unit tests for utils

### Task 1.4: Version Info
- [ ] Create OmniNet.cpp with version constant

---

## Implementation Status

| Phase | Component | Status |
|-------|-----------|--------|
| 1.1 | Build System | ✅ Complete |
| 1.2 | Exception & Logging | ⬜ In Progress |
| 1.3 | Utils | ⬜ Pending |
| 2 | Networking | ⬜ Pending |
| 3 | HTTP Protocol | ⬜ Pending |
| 4 | HTTP Server | ⬜ Pending |
| 5 | WebSocket | ⬜ Pending |
| 6 | Database | ⬜ Pending |
| 7 | Dashboard | ⬜ Pending |
| 8 | SSE | ⬜ Pending |
| 9 | Intrusion Detection | ⬜ Pending |
| 10 | Testing | ⬜ Pending |
| 11 | Demos | ⬜ Pending |

---

## Notes

- Build command: `cmake -S . -B build && cmake --build build -j$(nproc)`
- Static library output: `build/lib/libOmniNet.a`
- Tests output: `build/tests/`
