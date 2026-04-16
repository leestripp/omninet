# OmniNet - Project Documentation

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.8.0 |
| OS | Ubuntu 24.04 |
| Architecture | x86_64 |

## Project Info
- **Build System**: CMake with C++20
- **Dependencies**: OpenSSL, SQLite3, pthread
- **Test Framework**: Custom test macros (GTest optional)

## Analysis Summary (From prompt.txt)

### Bugs Found and Fixed

1. **HTTP keepAlive() Logic Bug** ✅ FIXED
   - **Location**: `src/http/HttpRequest.cpp`
   - **Issue**: `keepAlive()` returned true for HTTP/1.1 even when `Connection: close` was sent
   - **Root Cause**: The logic was `return conn == "keep-alive" || isHTTP11()` which always returned true for HTTP/1.1
   - **Fix**: Check for explicit "close" header first before falling back to HTTP version default
   - **Tests Added**: 24 new HTTP header parsing tests

### Test Results: 95 passed, 0 failed

| Category | Tests | Status |
|----------|-------|--------|
| Logger | 1 | ✅ Pass |
| Exception | 1 | ✅ Pass |
| Utils String | 10 | ✅ Pass |
| Utils URL Encoding | 4 | ✅ Pass |
| Utils UTF-8/Emoji | 4 | ✅ Pass |
| Utils Base64 | 4 | ✅ Pass |
| Utils Path (Security) | 5 | ✅ Pass |
| Utils MIME | 2 | ✅ Pass |
| Utils IP | 2 | ✅ Pass |
| Utils HTML (XSS) | 2 | ✅ Pass |
| Utils HMAC | 3 | ✅ Pass |
| Database | 3 | ✅ Pass |
| **HTTP Request (basic)** | 6 | ✅ Pass |
| **HTTP Request Headers (NEW)** | 24 | ✅ Pass |
| HTTP Request UTF-8 | 3 | ✅ Pass |
| HTTP Response | 4 | ✅ Pass |
| Router | 5 | ✅ Pass |
| Rate Limiter | 3 | ✅ Pass |
| Crypto | 4 | ✅ Pass |
| Password Hasher | 4 | ✅ Pass |

## Code Analysis Checklist

### ✅ HTTP Request Parsing
- [x] All HTTP methods (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
- [x] Query string parsing (single, multiple, empty, encoded params)
- [x] Header case-insensitivity
- [x] Content-Length parsing
- [x] Connection header (keep-alive vs close)
- [x] HTTP version (1.0 vs 1.1)
- [x] Path parsing (simple, query string, slash)
- [x] Edge cases (long paths, many headers, special chars)

### ✅ Security Features
- [x] Path traversal prevention (in Utils)
- [x] SQL injection prevention (parameterized queries)
- [x] XSS prevention (HTML escaping)
- [x] UTF-8/Emoji handling
- [x] Control characters in URLs

### Remaining Tasks
- [ ] Static file serving - verify CSS loading works
- [ ] Demo endpoints - test actual HTTP server endpoints
- [ ] WebSocket parsing tests
- [ ] SSE handler tests
- [ ] Integration tests

## Data Storage Design

### CWD (Current Working Directory) Approach

OmniNet stores all server files relative to the current working directory (CWD).

```
project_root/
├── static/               # Web assets (CSS, JS, images, index.html)
├── dashboard.db          # SQLite database (users, sessions)
├── logs/                # Application logs
└── certificates/        # TLS certificates
```

## Build & Run

```bash
# Build
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test

# Run demos (AVOID - causes endless loops)
# ./build/demos/http_server_demo
```
