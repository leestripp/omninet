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

## Bugs Found and Fixed

### 1. HTTP keepAlive() Logic Bug ✅ FIXED
- **File**: `src/http/HttpRequest.cpp`
- **Issue**: `Connection: close` header was ignored for HTTP/1.1 (always returned true)
- **Fix**: Check for explicit 'close' header first before falling back to HTTP version default

### 2. Connection Handler Blocking ✅ FIXED
- **File**: `src/http/HttpServer.cpp`
- **Issue**: `handleConnection()` processed connections synchronously, blocking the accept loop
- **Fix**: Spawn a new thread for each connection with `detach()` so accept loop can continue

### 3. Request Reading - recvAll() Blocking ✅ FIXED  
- **File**: `src/http/HttpConnection.cpp`
- **Issue**: `recvAll()` reads until EOF, blocking forever on keep-alive connections
- **Fix**: Rewrite `readRequest()` to read headers first (until double CRLF), then body if Content-Length > 0

## Test Results: 95 passed, 0 failed

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
| HTTP Request (basic) | 6 | ✅ Pass |
| HTTP Request Headers | 24 | ✅ Pass |
| HTTP Request UTF-8 | 3 | ✅ Pass |
| HTTP Response | 4 | ✅ Pass |
| Router | 5 | ✅ Pass |
| Rate Limiter | 3 | ✅ Pass |
| Crypto | 4 | ✅ Pass |
| Password Hasher | 4 | ✅ Pass |

## Build & Run

```bash
# Build
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test

# Run demo (test with curl)
./build/demos/http_server_demo &
sleep 2
curl http://localhost:8080/api/hello
curl http://localhost:8080/api/health
```

## Data Storage Design

OmniNet stores files relative to the current working directory (CWD):

```
project_root/
├── static/               # Web assets (CSS, JS, images)
├── dashboard.db          # SQLite database
├── logs/                # Application logs
└── certificates/        # TLS certificates
```
