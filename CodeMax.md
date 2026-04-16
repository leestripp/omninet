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

---

## TODO: Robust Configuration System

### Tasks

#### 1. Implement JSON Config File Loading ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- Implement `loadFromFile()` to parse JSON config files
- Support standard JSON structure with all config sections
- Add validation for required fields and value ranges
- Create default config file at `~/.omninet/config.json` if none exists

#### 2. Add Configuration Validation ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- Add `validate()` method to check config values are sensible
- Port numbers: 1-65535
- Timeouts: 1s - 3600s
- File paths: must exist or be creatable
- Worker count: 1-256 or 0 for auto

#### 3. Implement Config File Auto-Creation ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- On first run, create default `~/.omninet/config.json`
- Include all config sections with sensible defaults
- Include comments explaining each option
- Support migration from older config formats

#### 4. Add Hot Reload Support ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- Add `reload()` method to re-read config file
- File system watcher for automatic reload on change
- Thread-safe reloading without service interruption
- Callback system for components to react to config changes

#### 5. Extend Environment Variable Support ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- Full coverage of all config options via env vars
- Prefix: `OMNINET_` for global, `OMNINET_SERVER_`, etc. for sections
- Support arrays/lists via comma-separated values
- Boolean support: true/false, 1/0, yes/no

#### 6. Add Config CLI/Debug Tools ✅ DONE
- [x] **File**: `src/config/Config.cpp`
- `config dump` - print current config as JSON (via `toJson()`)
- `config validate` - check config file syntax (via `validate()`)
- `config init` - create default config file (via `createDefaultConfig()`)
- Pretty-print output for debugging

#### 7. Add Comprehensive Tests ✅ DONE
- [x] **File**: `tests/test_config.cpp`
- Test JSON parsing (valid and invalid files) ✅
- Test validation logic ✅
- Test environment variable overrides ✅
- Test hot reload functionality ✅
- Test default value handling ✅
- **14 new tests passing**

### Default Config Structure (`~/.omninet/config.json`)

```json
{
  "version": "1.0",
  "server": {
    "address": "0.0.0.0",
    "port": 8080,
    "workers": 0,
    "requestTimeout": 30,
    "keepAliveTimeout": 5,
    "maxRequestSize": 1048576
  },
  "tls": {
    "enabled": false,
    "certFile": "",
    "keyFile": "",
    "verifyClient": false
  },
  "cors": {
    "enabled": false,
    "allowedOrigins": "*",
    "allowedMethods": "GET,POST,PUT,DELETE",
    "allowedHeaders": "Content-Type,Authorization",
    "maxAge": 86400
  },
  "websocket": {
    "pingInterval": 30,
    "pongTimeout": 10,
    "maxMessageSize": 1048576,
    "maxConnections": 1024
  },
  "database": {
    "path": "omninet.db",
    "busyTimeoutMs": 5000,
    "foreignKeys": true,
    "walMode": true
  },
  "dashboard": {
    "enabled": true,
    "urlPath": "/omninet",
    "apiPath": "/omninet/api",
    "sessionTtl": 3600,
    "maxLoginAttempts": 5,
    "lockoutDuration": 300
  },
  "intrusion": {
    "enabled": true,
    "autoBlock": true,
    "blockDuration": 30,
    "maxFailedAttempts": 10,
    "attemptWindow": 15
  },
  "logging": {
    "level": "INFO",
    "file": "omninet.log",
    "toConsole": true,
    "toFile": true,
    "maxSizeMb": 10,
    "maxFiles": 5
  }
}
```

### Environment Variable Mapping

| Config Path | Environment Variable |
|-------------|---------------------|
| server.port | OMNINET_PORT |
| server.workers | OMNINET_WORKERS |
| server.requestTimeout | OMNINET_TIMEOUT |
| logging.level | OMNINET_LOG_LEVEL |
| tls.certFile | OMNINET_TLS_CERT |
| tls.keyFile | OMNINET_TLS_KEY |
| database.path | OMNINET_DATABASE |
| dashboard.enabled | OMNINET_DASHBOARD |

---

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

## Test Results: 124 passed, 0 failed

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
| Buffer | 15 | ✅ Pass |
| **Config System** | **14** | ✅ **NEW** |

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

OmniNet stores files in two locations:

### User Config Directory: `~/.omninet/`
```
~/.omninet/
├── config.json        # Main configuration file
├── omninet.db        # SQLite database (if not changed)
├── omninet.log       # Application logs (if file logging enabled)
└── certificates/     # TLS certificates (user-managed)
```

### Project/Relative Paths (resolved to data dir):
```
project_root/
├── static/           # Web assets (CSS, JS, images)
├── logs/             # Application logs
└── certificates/     # TLS certificates
```

### Config Loading Priority (highest to lowest):
1. Environment variables (`OMNINET_PORT`, `OMNINET_LOG_LEVEL`, etc.)
2. `~/.omninet/config.json` (user config)
3. Default values (hardcoded in code)
