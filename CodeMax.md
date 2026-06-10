# OmniNet - CodeMax Development Plan

## System Information
<!-- Auto-generated from system_info tool -->
| Component | Value |
|-----------|-------|
| Kernel | Linux 7.0.3-6.stable |
| OS | Ubuntu 26.04 LTS (Resolute Raccoon) |
| Container | podman (ubuntu:26.04) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |
| Session | Local, xterm-256color |
| User | lee |
| Hostname | dev-pc |

## Project Overview
- **Project**: OmniNet - Modern C++ HTTP/WebSocket Server Library
- **Version**: 0.3.0 (from git tag)
- **Type**: C++20 Library with CLI tools and demos
- **Build System**: CMake 3.16+
- **Repository**: /home/lee/projects/Websites/OmniNet

---

## Architecture Summary

```
OmniNet (Static Library)
├── Foundation (config, utils, exceptions)
├── Crypto (encryption, password hashing)
├── Networking (sockets, TLS, buffers)
├── HTTP Server (request/response, routing, static files)
├── WebSocket (RFC 6455 protocol)
├── Dashboard (admin UI, rate limiting)
├── Database (SQLite: sessions, users)
├── Vector Store (sqlite-vec integration)
├── SSE (Server-Sent Events)
└── Intrusion Detection (security monitoring)
```

---

## Directory Structure

```
OmniNet/
├── src/                    # Source code (library)
│   ├── OmniNet.h/cpp       # Main header + init
│   ├── config/             # Configuration system
│   ├── crypto/              # Crypto & password hashing
│   ├── db/                  # SQLite database layer
│   ├── dashboard/           # Admin dashboard & rate limiting
│   ├── http/                # HTTP server (core)
│   ├── intrusion/           # Security/intrusion detection
│   ├── net/                 # Sockets, TLS, buffers
│   ├── sse/                 # Server-Sent Events
│   ├── token/               # Token management (Session Tokens, API Keys) ⭐
│   ├── utils/               # Logging, exceptions, utilities
│   ├── vector/              # Vector store (sqlite-vec)
│   ├── websocket/           # WebSocket protocol
│   └── admin/               # Admin handler
├── include/                 # Public headers (OmniNet/)
├── tests/                   # Unit tests
├── demos/                   # Demo applications
├── tools/                   # CLI tools
│   ├── omninet-status       # Server status
│   ├── omninet-health       # Health checks
│   ├── omninet-init         # Initialization
│   ├── omninet-users        # User management
│   ├── omninet-certs        # TLS certificates
│   ├── omninet-db           # Database tools
│   ├── omninet-log          # Log viewer
│   ├── omninet-firewall      # Firewall management
│   ├── omninet-ctl           # Server control
│   └── omninet-mon           # Monitoring
├── cmake/                   # CMake modules
├── docs/                    # Documentation
├── static/                  # Static web files (dashboard)
├── third_party/             # Third-party sources
├── CMakeLists.txt           # Main build configuration
└── CodeMax.md               # This file
```

---

## Dependencies

| Dependency | Version | Purpose | Fetch Method |
|------------|---------|---------|--------------|
| OpenSSL | 3.5.5 | TLS/SSL encryption | System |
| SQLite3 | 3.46.1 | Database | System |
| nlohmann/json | 3.11.3 | JSON parsing | FetchContent |
| sqlite-vec | 0.1.6 | Vector storage | FetchContent |
| pthread | - | Threading | System |
| zlib | - | Gzip compression | Optional |

---

## Build System

### Build Commands
```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test

# Run CTest
ctest --test-dir build --output-on-failure

# Static analysis
cmake --build build --target cppcheck
```

### Build Status
- **Library**: OmniNet (static)
- **Demos**: http_server_demo, https_server_demo, dashboard_demo, static_files_demo, websocket_echo_demo
- **Tools**: 10 CLI tools (omninet-status, omninet-init, etc.)
- **Tests**: 110 unit tests (100% passing)
- **Status**: ✅ All builds passing

---

## Source Code Modules

### Core HTTP Module (`src/http/`)
| File | Lines | Purpose |
|------|-------|---------|
| HttpRequest.h/cpp | 768 | HTTP request parsing & handling |
| HttpResponse.h/cpp | 1021 | HTTP response generation |
| HttpConnection.h/cpp | 590 | Connection handling |
| HttpServer.h/cpp | 584 | Main HTTP server |
| Router.h/cpp | 604 | URL routing & middleware |
| StaticHandler.h/cpp | 415 | Static file serving |

### Database Module (`src/db/`)
| File | Purpose |
|------|---------|
| Database.h/cpp | SQLite wrapper with transactions |
| SessionStore.h/cpp | Session management with TTL |
| UserStore.h/cpp | User authentication & RBAC |

### Networking Module (`src/net/`)
| File | Purpose |
|------|---------|
| Socket.h/cpp | TCP socket wrapper |
| TLSSocket.h/cpp | TLS/SSL socket wrapper |
| Buffer.h/cpp | Binary buffer with dynamic growth |

---

## Known Issues & Technical Debt

### Security
1. **SQL Injection Risk** - SessionStore/UserStore use string concatenation instead of prepared statements for user-provided IDs
2. **Input Validation** - Some endpoints may lack proper input validation

### Architecture
1. **Tools vs Library Duplication** - Separate database implementations by design:
   - `src/db/Database` - Rich feature set (prepared statements, transactions)
   - `tools/tools_common/database` - Simple wrapper for standalone tools
   - Consolidation would require tools to link full library (not desired)
   - This is an architectural trade-off, not a bug
2. **Gzip Enabled** - zlib found, compression available ✅

---

## Tasks

### Completed ✅
- [x] Build system setup (CMake)
- [x] Core HTTP server implementation
- [x] WebSocket support (RFC 6455)
- [x] TLS/HTTPS support
- [x] Session management (SQLite)
- [x] User authentication
- [x] Admin dashboard
- [x] CLI tools (10 tools)
- [x] Demos (5 demos)
- [x] Unit tests (110 tests)
- [x] sqlite-vec integration
- [x] VectorStore class implementation
- [x] Cppcheck static analysis targets
- [x] **Token System**: Session Tokens + API Keys (28 tests)

### In Progress 🔄
- None

### TODO 📋
- [x] **Phase 1**: Token Utilities (`src/token/TokenUtils.h/cpp`) ✅
- [x] **Phase 2**: Session Token Store (`src/token/SessionTokenStore.h/cpp`) ✅
- [x] **Phase 3**: API Key Store (`src/token/ApiKeyStore.h/cpp`) ✅
- [x] **Phase 4**: Unit tests for token system ✅ (28 tests)
- [x] **Documentation**: Updated README.md and created docs/api/token.md
- [x] Fix SQL injection in SessionStore/UserStore ✅
- [x] Consolidate tools/library database implementations ✅
- [x] Enable gzip compression (install zlib) ✅

---

## Token System Implementation Plan ⭐

### Overview
Add robust **Session Token** and **API Key** management to OmniNet using existing dependencies (SQLite3, OpenSSL, Crypto utilities).

### Dependencies Used
- **OpenSSL** - Cryptographically secure random generation
- **SQLite3** - Persistent token storage
- **Existing Crypto utils** - `SecureToken`, `Crypto::randomBytes`, `Crypto::hmacSha256`

---

### Part 1: Token Utilities (`src/token/TokenUtils.h/cpp`)

#### Goals
- Generate secure random tokens (hex/base32)
- Token hashing utilities (SHA-256)
- Format validation
- Constant-time comparison

#### API Design
```cpp
namespace OmniNet::Token {

class TokenUtils {
public:
    // Generate secure random token (hex encoded)
    static std::string generate(size_t byteLength = 32);
    
    // Generate API key with prefix
    static std::string generateApiKey(std::string_view prefix);
    
    // Hash token (SHA-256)
    static std::string hashToken(std::string_view token);
    
    // Extract prefix from API key
    static std::string extractPrefix(std::string_view apiKey);
    
    // Extract secret from API key
    static std::string extractSecret(std::string_view apiKey);
    
    // Validate token format (hex, expected length)
    static bool isValidFormat(std::string_view token, size_t expectedBytes);
    
    // Validate API key format (prefix_secret)
    static bool isValidApiKeyFormat(std::string_view apiKey);
    
    // Constant-time comparison for token validation
    static bool secureCompare(std::string_view a, std::string_view b);
};

}
```

#### Unit Tests (~20 test cases)
| Test | Description |
|------|-------------|
| `generate_hex_length` | Generate produces expected length |
| `generate_hex_charset` | Only hex characters produced |
| `generate_uniqueness` | 10000 generates all unique |
| `generate_api_key_format` | API key has correct format |
| `hash_token_deterministic` | Same input = same output |
| `hash_token_different` | Different input = different output |
| `extract_prefix` | Prefix extraction works |
| `extract_secret` | Secret extraction works |
| `is_valid_format` | Format validation works |
| `is_valid_api_key_format` | API key format validation |
| `secure_compare_equal` | Equal strings compare true |
| `secure_compare_different_length` | Different lengths = false |
| `secure_compare_different_content` | Different content = false |

---

### Part 2: Session Token Store (`src/token/SessionTokenStore.h/cpp`)

#### Goals
- Generate cryptographically secure session tokens
- Unique token IDs with collision resistance
- Token validation with expiry
- Token revocation
- Token metadata (user, created, expires, last used, IP)
- SQL injection protection (prepared statements)

#### Database Schema
```sql
CREATE TABLE session_tokens (
    id          TEXT PRIMARY KEY,           -- Token ID (64 hex chars)
    token_hash  TEXT NOT NULL UNIQUE,       -- SHA-256 hash of full token
    user_id     TEXT NOT NULL,              -- Associated user
    created_at  TEXT NOT NULL,              -- ISO-8601 timestamp
    expires_at  TEXT NOT NULL,              -- Expiration timestamp
    last_used_at TEXT,                      -- Last usage timestamp
    last_ip     TEXT,                       -- Last IP address
    user_agent  TEXT,                       -- Last user agent
    revoked     INTEGER DEFAULT 0,          -- Revocation flag
    revoked_at  TEXT,                      -- Revocation timestamp
    metadata    TEXT                        -- JSON metadata
);

CREATE INDEX idx_tokens_user ON session_tokens(user_id);
CREATE INDEX idx_tokens_expires ON session_tokens(expires_at);
CREATE INDEX idx_tokens_hash ON session_tokens(token_hash);
```

#### Token Format
```
Format: <random_bytes_base16>
Length: 32 bytes (64 hex characters)
Example: a3f8b2c1d4e5f6789012345678901234567890abcdef1234567890abcdef123456
```

#### API Design
```cpp
namespace OmniNet::Token {

struct SessionToken {
    std::string id;                                      // Token ID
    std::string userId;                                 // Associated user
    std::chrono::system_clock::time_point createdAt;    // Creation time
    std::chrono::system_clock::time_point expiresAt;    // Expiration time
    std::optional<std::chrono::system_clock::time_point> lastUsedAt;
    std::optional<std::string> lastIp;
    std::optional<std::string> userAgent;
    bool revoked = false;
    std::optional<std::chrono::system_clock::time_point> revokedAt;
    std::string metadata;                                // JSON string
};

class SessionTokenStore {
public:
    SessionTokenStore(DB::Database& db);
    void initialize();
    
    // Create new token (returns raw token - only time it's visible)
    std::string create(
        std::string_view userId,
        std::chrono::seconds ttl = std::chrono::hours(24),
        std::string_view metadata = {}
    );
    
    // Create with additional metadata
    std::string create(
        std::string_view userId,
        std::chrono::seconds ttl,
        std::string_view ip,
        std::string_view userAgent,
        std::string_view metadata = {}
    );
    
    // Validate token
    std::optional<SessionToken> validate(std::string_view rawToken);
    
    // Validate and update usage
    std::optional<SessionToken> validateAndTouch(
        std::string_view rawToken,
        std::string_view currentIp
    );
    
    // Revoke a single token
    bool revoke(std::string_view rawToken);
    
    // Revoke all tokens for a user
    size_t revokeAllForUser(std::string_view userId);
    
    // Get token by ID
    std::optional<SessionToken> getById(std::string_view tokenId);
    
    // List all active tokens for a user
    std::vector<SessionToken> listForUser(std::string_view userId);
    
    // Remove expired tokens
    size_t cleanup();
    
    // Statistics
    size_t count() const;
    size_t countActive() const;
    size_t countForUser(std::string_view userId) const;
};

}
```

#### Key Features
1. **Secure Generation**: 32 bytes from `Crypto::randomBytes()`, stored as hex
2. **Token Hashing**: Store SHA-256 hash, never store raw token
3. **Validation**: Lookup by token hash, check expiry and revocation
4. **Revocation**: Mark as revoked, don't delete (audit trail)
5. **Usage Tracking**: Update last_used_at, last_ip on each validation
6. **Cleanup**: Background cleanup of expired tokens

#### Unit Tests (~15 test cases)
| Test | Description |
|------|-------------|
| `create_basic` | Create token and verify stored |
| `create_with_metadata` | Create with IP, user agent |
| `create_uniqueness` | Verify 1000 tokens are all unique |
| `validate_valid` | Validate fresh token returns session |
| `validate_expired` | Expired token returns nullopt |
| `validate_revoked` | Revoked token returns nullopt |
| `validate_updates_usage` | Touch updates last_used_at, last_ip |
| `revoke_single` | Revoking one token doesn't affect others |
| `revoke_all_for_user` | Revoking all tokens for user |
| `list_for_user` | List all tokens for user |
| `cleanup_expired` | Cleanup removes expired tokens |
| `hash_not_stored` | Raw token not in database |
| `invalid_format` | Invalid token format returns nullopt |

---

### Part 3: API Key Store (`src/token/ApiKeyStore.h/cpp`)

#### Goals
- Generate prefixed API keys for identification
- Store hashed keys (never store raw)
- Support scopes/permissions
- Usage tracking (optional)
- Expiration support
- Key rotation support
- SQL injection protection

#### Database Schema
```sql
CREATE TABLE api_keys (
    id              TEXT PRIMARY KEY,           -- Key ID (8 hex chars, public prefix)
    key_hash        TEXT NOT NULL,              -- SHA-256 hash of full key
    key_prefix      TEXT NOT NULL,              -- First 8 chars for identification
    user_id         TEXT NOT NULL,              -- Owner user ID
    name            TEXT NOT NULL,              -- Human-readable name
    scopes          TEXT NOT NULL,              -- JSON array of scopes
    created_at      TEXT NOT NULL,              -- Creation timestamp
    expires_at      TEXT,                       -- Expiration timestamp (null = never)
    last_used_at    TEXT,                       -- Last usage timestamp
    last_ip         TEXT,                       -- Last IP address
    revoked         INTEGER DEFAULT 0,         -- Revocation flag
    revoked_at      TEXT,                       -- Revocation timestamp
    use_count       INTEGER DEFAULT 0,          -- Total usage count
    use_limit       INTEGER,                    -- Max uses (null = unlimited)
    metadata        TEXT                        -- JSON metadata
);

CREATE INDEX idx_keys_user ON api_keys(user_id);
CREATE INDEX idx_keys_hash ON api_keys(key_hash);
CREATE INDEX idx_keys_prefix ON api_keys(key_prefix);
```

#### Key Format
```
Format: <prefix>_<random_base32>
Length: 8 + 1 + 24 = 33 characters
Example: omn_sk_a3f8b2c1d4e5f6789012345678

Parts:
  - omn_sk_  : Prefix (8 chars, identifies key type/owner)
  - a3f8...  : Random bytes (base32 encoded, 24 chars)
```

#### API Design
```cpp
namespace OmniNet::Token {

struct ApiKey {
    std::string id;                           // Public key ID (prefix)
    std::string userId;                       // Owner user ID
    std::string name;                         // Human-readable name
    std::vector<std::string> scopes;          // Permission scopes
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> expiresAt;
    std::optional<std::chrono::system_clock::time_point> lastUsedAt;
    std::optional<std::string> lastIp;
    bool revoked = false;
    std::optional<std::chrono::system_clock::time_point> revokedAt;
    uint64_t useCount = 0;
    std::optional<uint64_t> useLimit;
    std::string metadata;                     // JSON string
};

class ApiKeyStore {
public:
    ApiKeyStore(DB::Database& db);
    void initialize();
    
    // Create result
    struct CreateResult {
        std::string rawKey;      // Full key (only shown once!)
        std::string id;          // Public ID (prefix)
    };
    
    // Create new API key
    CreateResult create(
        std::string_view userId,
        std::string_view name,
        std::vector<std::string> scopes,
        std::chrono::seconds ttl = std::chrono::seconds::zero()
    );
    
    // Create with additional options
    CreateResult create(
        std::string_view userId,
        std::string_view name,
        std::vector<std::string> scopes,
        std::chrono::seconds ttl,
        std::string_view ip,
        std::string_view userAgent,
        std::optional<uint64_t> useLimit,
        std::string_view metadata = {}
    );
    
    // Validation result
    struct ValidationResult {
        bool valid;
        std::optional<ApiKey> key;
        std::string error;  // "expired", "revoked", "not_found", "usage_limit"
    };
    
    // Validate API key
    ValidationResult validate(std::string_view rawKey);
    
    // Validate and update usage
    ValidationResult validateAndUse(
        std::string_view rawKey,
        std::string_view currentIp
    );
    
    // Check if key has required scope
    bool hasScope(const ApiKey& key, std::string_view requiredScope);
    
    // Revoke by public ID
    bool revoke(std::string_view keyId);
    
    // Revoke all keys for user
    size_t revokeAllForUser(std::string_view userId);
    
    // Queries
    std::optional<ApiKey> getById(std::string_view keyId);
    std::vector<ApiKey> listForUser(std::string_view userId);
    
    // Cleanup expired keys
    size_t cleanup();
    
    // Statistics
    size_t count() const;
    size_t countActive() const;
    size_t countForUser(std::string_view userId) const;
};

}
```

#### Key Features
1. **Key Prefix**: First 8 chars stored for identification without exposure
2. **Key Hashing**: Store SHA-256 hash, verify by hashing input and comparing
3. **Scopes**: JSON array of permission scopes (e.g., `["read", "write"]`)
4. **Expiration**: Optional expiration timestamp
5. **Usage Limits**: Optional max use count
6. **Usage Tracking**: Track use_count, last_used_at, last_ip
7. **Revocation**: Mark as revoked, don't delete

#### Unit Tests (~20 test cases)
| Test | Description |
|------|-------------|
| `create_basic` | Create API key and verify |
| `create_with_prefix` | Verify prefix in key format |
| `create_uniqueness` | Verify 1000 keys are unique |
| `create_with_scopes` | Create with multiple scopes |
| `create_with_expiry` | Create with expiration |
| `create_with_use_limit` | Create with max uses |
| `validate_valid` | Validate fresh key |
| `validate_expired` | Expired key returns error |
| `validate_revoked` | Revoked key returns error |
| `validate_usage_limit` | Key at limit returns error |
| `validate_updates_usage` | Usage updates count, last_ip |
| `has_scope` | Scope checking works |
| `has_scope_missing` | Missing scope returns false |
| `revoke_by_id` | Revoke using public ID |
| `list_for_user` | List all keys for user |
| `cleanup_expired` | Cleanup removes expired keys |
| `prefix_stored_separately` | Prefix stored, raw key hashed |

---

### Part 4: File Structure (Final)

```
src/token/
├── TokenUtils.h            # Token generation utilities
├── TokenUtils.cpp
├── SessionToken.h          # Session token structures
├── SessionTokenStore.h     # Session token store
├── SessionTokenStore.cpp
├── ApiKey.h                # API key structures
├── ApiKeyStore.h           # API key store
└── ApiKeyStore.cpp

tests/
├── test_token_session.cpp  # Session token tests
├── test_token_api_key.cpp  # API key tests
├── test_token_utils.cpp    # Token utility tests
└── CMakeLists.txt          # UPDATE
```

---

### Security Considerations

| Feature | Protection |
|---------|------------|
| Cryptographic RNG | 32 bytes from OpenSSL's `RAND_bytes()` |
| Token Hashing | SHA-256 hash stored, never raw |
| Timing Attack Prevention | OpenSSL `CRYPTO_memcmp()` |
| SQL Injection | Prepared statements for all queries |
| Prefix for API Keys | Only 8-char prefix stored publicly |

---

### Estimated Effort

| Phase | Tasks | Files | Lines | Time |
|-------|-------|-------|-------|------|
| **Phase 1** | TokenUtils | 3 | ~200 | 1-2 hours |
| **Phase 2** | SessionTokenStore | 4 | ~400 | 2-3 hours |
| **Phase 3** | ApiKeyStore | 4 | ~500 | 2-3 hours |
| **Phase 4** | Integration | 2 | ~100 | 1 hour |
| **Tests** | Unit tests | 4 | ~600 | 2-3 hours |
| **Total** | | ~17 | ~1800 | 8-12 hours |

---

### Usage Examples

#### Creating a Session Token
```cpp
SessionTokenStore store(db);

// Create token for user
std::string rawToken = store.create(
    "user_123",
    std::chrono::hours(24),      // TTL
    "192.168.1.1",               // IP
    "Mozilla/5.0",                // User-Agent
    R"({"purpose": "web"})"       // Metadata
);

// Later, validate token
auto session = store.validate(rawToken);
if (session) {
    std::cout << "User: " << session->userId << "\n";
}
```

#### Creating an API Key
```cpp
ApiKeyStore store(db);

auto result = store.create(
    "user_123",
    "Production API Key",
    {"read", "write:orders"},     // Scopes
    std::chrono::days(365)        // Expires in 1 year
);

// Store rawKey securely (show to user ONCE!)
std::cout << "API Key: " << result.rawKey << "\n";

// Later, validate API key
auto validation = store.validate(result.rawKey);
if (validation.valid && store.hasScope(*validation.key, "read")) {
    // Allow read operation
}
```

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2026-05-23 | 0.3.2 | Enable zlib/gzip compression |
| 2026-05-23 | 0.3.1 | Token system: SessionTokenStore + ApiKeyStore |
| 2026-05-23 | 0.3.0 | Current version from git tag |
| 2026-05-22 | - | Implemented sqlite-vec FetchContent build |
| 2026-05-22 | - | Simplified vector plan (embeddings out of scope) |
| 2026-05-15 | - | Fixed SQLite CMake target name |
| 2026-05-04 | - | Added Cppcheck static analysis |
| 2026-04-18 | - | Initial HTTP/WebSocket implementation |

---

## Testing

### Test Framework
- **Framework**: Custom header-only test framework
- **Location**: `tests/test_utils.cpp`
- **Tests**: 110 passing tests across all modules

### Test Categories
- Configuration tests
- Logger tests
- Database tests
- HTTP request/response tests
- WebSocket tests
- Socket tests
- Crypto tests
- Router tests
- Buffer protection tests

---

## CLI Tools Usage

```bash
# Initialize new installation
./build/tools/omninet-init

# Check server status
./build/tools/omninet-status

# Health check
./build/tools/omninet-health

# User management
./build/tools/omninet-users add admin password
./build/tools/omninet-users list

# TLS certificates
./build/tools/omninet-certs generate

# Database operations
./build/tools/omninet-db info

# Log viewer
./build/tools/omninet-log tail
./build/tools/omninet-log search --level ERROR

# Server control
./build/tools/omninet-ctl start
./build/tools/omninet-ctl stop

# Firewall management
./build/tools/omninet-firewall status
./build/tools/omninet-firewall block 192.168.1.100

# Monitoring
./build/tools/omninet-mon --interval 5
```

---

## Code Style Guidelines

- **Standard**: C++20
- **Naming**: PascalCase for classes/functions, camelCase for variables
- **Headers**: Use include guards, avoid namespace pollution
- **Memory**: Prefer smart pointers, RAII patterns
- **Error Handling**: Exceptions with descriptive messages
- **Logging**: LogLevel enum (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)

---

## Session Memory

- **Builds**: 499 total
- **Sessions**: 5 total
- **Errors Resolved**: 0/0

### Recent Fixes
- CMake version detection improved (git tags)
- SQLite CMake target compatibility fixed
- VectorStore class implemented with sqlite-vec

---

## Current Status

| Component | Status |
|-----------|--------|
| Build | ✅ Passing |
| Tests | ✅ 110/110 passing |
| Library | ✅ OmniNet static library |
| Demos | ✅ 5 demos |
| Tools | ✅ 10 CLI tools |
| Vector | ✅ sqlite-vec integration |
| Static Analysis | ✅ cppcheck targets |
| Token System | ✅ Implemented |
| Compression | ✅ zlib enabled |

**Last Updated**: 2026-05-23 14:00 AEST

---

## Future Work (Potential)

| Feature | Description | Priority |
|---------|-------------|----------|
| ~~Gzip response compression~~ | ~~Implement in HttpResponse for `Accept-Encoding: gzip`~~ ✅ Done | Medium |
| **C++23 + Async + ThreadPool Refactor** | See refactor plan below | **High** |
| Rate limiting middleware | Global rate limiter with Redis support | Medium |
| WebSocket load balancing | Session affinity or stateless mode | Low |
| HTTP/2 support | Upgrade path for performance | Low |
| Metrics export | Prometheus/Graphite integration | Low |

---

## C++23 / Async / ThreadPool Refactor Plan

**Status**: 📋 Planning  
**Target Version**: 0.4.0  
**Estimated Effort**: 6-8 weeks (incremental, backward-compatible)

### Executive Summary

OmniNet currently uses a **one-thread-per-connection** model that doesn't scale.
The plan upgrades the library to **C++23** and introduces:
- A real **ThreadPool** with work-stealing
- **Async/callback-based handlers** (C++20 coroutines + `std::move_only_function`)
- **Structured concurrency** via `std::jthread` and stop tokens
- **Modern error handling** with `std::expected<T, E>`
- **Non-blocking DB** operations with connection pooling

The migration is **incremental** — existing synchronous code continues to work.

---

### Current State Analysis

| Issue | Location | Impact |
|-------|----------|--------|
| One-thread-per-connection | `HttpServer.cpp:226` (`std::thread(...).detach()`) | Doesn't scale past ~1k connections |
| Idle worker threads | `HttpServer.cpp:283-287` | `m_workers` do nothing; sleep loop |
| No async I/O | `HttpConnection.cpp:readRequest/sendResponse` | Blocking calls block threads |
| No coroutine support | All handlers | C++20 coroutines unused |
| Synchronous DB | `Database.cpp` | Every query blocks a thread |
| Manual thread mgmt | `HttpServer.cpp:80` | Uses `std::thread` + manual join |
| C++20 only | `CMakeLists.txt:65` | Missing C++23 features |
| `gzip` not integrated | `HttpResponse.cpp:311` | Compression exists but unused |

**Lines of code**: ~18,559 across 47 source files.

---

### Refactor Goals

1. **Scale to 10,000+ concurrent connections** on a single thread pool
2. **Reduce thread count** from N+1 to N (where N = CPU cores)
3. **Modern C++23** idioms throughout
4. **Drop-in compatible** for existing users
5. **Backpressure** for runaway connections
6. **Graceful shutdown** with stop tokens

---

### Phase 1: Build System Upgrade to C++23

#### Files to Modify
- `CMakeLists.txt` — change C++ standard
- `cmake/CompilerWarnings.cmake` — update warning flags

#### Changes
```cmake
# CMakeLists.txt
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # Modules off for now

# Add required flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(OmniNet PRIVATE -fcoroutines)
endif()
```

#### Key C++23 Features Used
| Feature | Purpose |
|---------|---------|
| `std::expected<T, E>` | Error handling without exceptions |
| `std::move_only_function` | Type-erased callbacks (no `std::function` overhead) |
| `std::flat_map` | Faster header storage |
| Deducing `this` | Cleaner CRTP patterns |
| `if consteval` | Compile-time branches |
| `std::stacktrace` | Better error reporting |
| `std::ranges` improvements | More expressive code |
| `std::span` improvements | Better view semantics |
| `std::byteswap` | Endianness handling |

#### Verification
- All 110 existing tests must pass
- GCC 13+ / Clang 17+ required
- Update minimum compiler in README

---

### Phase 2: ThreadPool System

#### New Module: `src/thread/`

```
src/thread/
├── ThreadPool.h          # Public API
├── ThreadPool.cpp        # Implementation
├── WorkStealingQueue.h   # Per-thread work queue
├── Task.h                # Task type-erased wrapper
└── StopToken.h           # Cancellation primitives
```

#### API Design

```cpp
namespace OmniNet::Thread {

// Forward declarations
class ThreadPool;
class WorkerThread;

// Task type-erased with std::move_only_function (C++23)
using Task = std::move_only_function<void()>;

class ThreadPool {
public:
    struct Config {
        size_t workerCount = 0;        // 0 = hardware_concurrency()
        size_t maxQueueSize = 1024;    // Backpressure
        bool stealWork = true;         // Work stealing enabled
    };
    
    explicit ThreadPool(Config config = {});
    ~ThreadPool();
    
    // Submit a task (returns future for results)
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    // Submit a coroutine
    auto submit_task(std::coroutine_handle<> h) -> void;
    
    // Schedule with delay
    template<typename Rep, typename Period, typename F>
    auto submit_after(std::chrono::duration<Rep, Period> delay, F&& f);
    
    // Graceful shutdown (waits for tasks)
    void shutdown();
    
    // Statistics
    struct Stats {
        size_t workerCount;
        size_t activeTasks;
        size_t queuedTasks;
        uint64_t totalExecuted;
        uint64_t totalStolen;
    };
    Stats stats() const;
    
private:
    std::vector<std::jthread> m_workers;
    // Per-worker lockfree queues (boost::lockfree or moodycamel)
    // Global overflow queue
    // Work-stealing logic
    std::stop_source m_stopSource;
};

}
```

#### Implementation Details

**Work-Stealing Queues**:
- Use `boost::lockfree::queue` or `moodycamel::ConcurrentQueue` per worker
- Global overflow queue when local queues full
- Idle workers steal from neighbors (random selection)

**Stop Token Integration**:
```cpp
class WorkerThread {
    void run(std::stop_token st) {
        while (!st.stop_requested()) {
            if (auto task = dequeue(); task) {
                task();
            } else {
                wait_for_work(st);  // sleep or steal
            }
        }
    }
};
```

**Backpressure**:
- `submit()` blocks if queue exceeds `maxQueueSize`
- Optional timeout via `submit_for(...)` 
- Rejected tasks throw `std::runtime_error`

#### Unit Tests (~15 test cases)

| Test | Description |
|------|-------------|
| `pool_basic_submit` | Submit and execute simple task |
| `pool_concurrent_submit` | 1000 tasks from 4 threads |
| `pool_future_result` | Return values via std::future |
| `pool_exception_propagation` | Exceptions in tasks reach future |
| `pool_stop_request` | Stop token cancels workers |
| `pool_graceful_shutdown` | Wait for in-flight tasks |
| `pool_work_stealing` | Verify stealing actually happens |
| `pool_backpressure` | Block when queue full |
| `pool_priority_queue` | (Optional) Priority tasks |
| `pool_scheduled_task` | Delayed task execution |
| `pool_thread_affinity` | (Optional) Pin to cores |
| `pool_no_allocation` | (Optional) No allocation in hot path |

#### Estimated Effort
- 2-3 days implementation
- 1 day testing
- 1 day benchmarking

---

### Phase 3: Async Handler Infrastructure

#### New Module: `src/async/`

```
src/async/
├── Task.h              # C++20 coroutine task type
├── Promise.h           # Promise type for coroutines
├── Awaitable.h         # Awaiter interfaces
├── SocketStream.h      # Async socket I/O awaiters
├── AsyncHandler.h      # Handler signature with future
└── Executor.h          # Abstract executor interface
```

#### C++20 Coroutine Task

```cpp
namespace OmniNet::Async {

// C++20 coroutine return type
template<typename T = void>
class Task {
public:
    struct promise_type {
        Task get_return_object();
        std::suspend_never initial_suspend();
        std::suspend_always final_suspend() noexcept;
        template<typename U>
        void return_value(U&& value);
        void return_void();
        void unhandled_exception();
        
        std::coroutine_handle<> continuation = nullptr;
        std::optional<T> result;
        std::exception_ptr error;
    };
    
    // Awaiter interface
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<>) noexcept;
    T await_resume();
    
    // Check if completed
    bool is_ready() const noexcept;
    
    // Get result (throws if error)
    T result();
    
private:
    std::coroutine_handle<promise_type> m_handle;
};

// Synchronous wrapper for old-style handlers
template<typename T>
T blocking_get(Task<T> task);
```

#### Async Handler API

```cpp
namespace OmniNet::HTTP {

// New async handler signature
using AsyncHandler = std::function<Async::Task<Response>(
    const Request&, 
    Async::Executor&  // Inject executor
)>;

// Backward compat: sync handlers auto-wrapped
using Handler = std::function<void(const Request&, Response&)>;

class Router {
public:
    // Old sync route (still supported)
    void get(std::string pattern, Handler handler);
    
    // New async route
    void get(std::string pattern, AsyncHandler handler);
    
    // Universal: accepts both
    void get(std::string pattern, std::move_only_function<void(const Request&, Response&)> handler);
};

}
```

#### Awaitable Socket I/O

```cpp
namespace OmniNet::Async {

// Awaitable async read
class SocketReadAwaiter {
public:
    SocketReadAwaiter(Socket& sock, std::span<uint8_t> buf);
    
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h);
    size_t await_resume();  // bytes read
    
private:
    Socket& m_socket;
    std::span<uint8_t> m_buffer;
    // Overlapped I/O state (Windows) / epoll state (Linux)
};

}
```

#### Usage Example

```cpp
// Sync handler (unchanged)
server.get("/sync", [](const Request& req, Response& resp) {
    resp.setBody("Hello, world!");
});

// Async handler (new)
server.get("/async", [](const Request& req, Async::Executor& exec) 
    -> Async::Task<Response> {
    // Simulate async DB query
    auto data = co_await exec.submit_async([]() {
        return db.query("SELECT ...");
    });
    
    Response resp;
    resp.setBody(std::move(data));
    co_return resp;
});
```

#### Unit Tests (~10 test cases)

| Test | Description |
|------|-------------|
| `task_basic_coroutine` | Simple co_return value |
| `task_exception_propagation` | Throw inside coroutine |
| `task_await_chain` | co_await another task |
| `task_cancel` | Cancellation via stop_token |
| `task_timeout` | Awaitable with timeout |
| `handler_sync_compat` | Old sync handler works |
| `handler_async_dispatch` | Async handler executes |
| `handler_mixed_routes` | Mix of sync and async |
| `handler_thread_affinity` | Async runs on pool |
| `handler_error_recovery` | Errors caught gracefully |

#### Estimated Effort
- 4-5 days implementation
- 2 days testing
- 1 day documentation

---

### Phase 4: Non-Blocking I/O Layer

#### New Module: `src/io/`

```
src/io/
├── IoContext.h           # Abstract I/O multiplexer
├── EpollContext.h        # Linux epoll backend
├── KqueueContext.h       # BSD/macOS kqueue backend
├── IocpContext.h         # Windows IOCP backend
├── AsyncSocket.h         # Coroutine-friendly socket
├── AsyncTimer.h          # Timers as awaitables
└── AsyncBuffer.h         # Async I/O buffers
```

#### Architecture

```
        ┌──────────────────────┐
        │  Async::Task<T>      │  ← User code
        └──────────┬───────────┘
                   │ co_await
        ┌──────────▼───────────┐
        │  AsyncSocket         │  ← Awaiter interface
        │  AsyncTimer          │
        └──────────┬───────────┘
                   │ read/write
        ┌──────────▼───────────┐
        │  IoContext (epoll)   │  ← Platform-specific
        │  + ThreadPool        │
        └──────────────────────┘
```

#### IoContext API

```cpp
namespace OmniNet::IO {

class IoContext {
public:
    IoContext(ThreadPool& pool);
    ~IoContext();
    
    // Run event loop (blocking, returns when stopped)
    void run();
    
    // Run with stop token
    void run(std::stop_token st);
    
    // Schedule callback on next event loop iteration
    void post(Task task);
    
    // Register socket for read events
    void watch_read(Socket& sock, Task on_read);
    
    // Register socket for write events
    void watch_write(Socket& sock, Task on_write);
    
    // Stop all I/O
    void stop();
};

}
```

#### AsyncSocket API

```cpp
namespace OmniNet::IO {

class AsyncSocket {
public:
    explicit AsyncSocket(IoContext& ctx, std::unique_ptr<Socket> sock);
    
    // Async read (co_await)
    Async::Task<std::span<uint8_t>> read_some(std::span<uint8_t> buf);
    Async::Task<std::vector<uint8_t>> read_until(std::string_view delimiter);
    
    // Async write
    Async::Task<size_t> write_some(std::span<const uint8_t> data);
    Async::Task<void> write_all(std::span<const uint8_t> data);
    
    // Async accept (server side)
    Async::Task<std::unique_ptr<AsyncSocket>> accept();
    
    // Cancel pending operations
    void cancel();
    
    Socket& native() { return *m_socket; }
    
private:
    IoContext& m_context;
    std::unique_ptr<Socket> m_socket;
};

}
```

#### Platform-Specific Notes

**Linux** (epoll):
- Edge-triggered mode for performance
- One event loop thread + thread pool for handlers
- `io_uring` for future enhancement

**Windows** (IOCP):
- Overlapped I/O with completion callbacks
- Use `GetQueuedCompletionStatusEx` for batched completions
- `WSASend`/`WSARecv` for I/O

**macOS/BSD** (kqueue):
- EVFILT_READ/EVFILT_WRITE filters
- `kqueue()` + `kevent()`

#### Unit Tests (~12 test cases)

| Test | Description |
|------|-------------|
| `io_context_run_stop` | Event loop stops on stop request |
| `io_context_post` | post() runs in event loop |
| `async_socket_read` | Read async data |
| `async_socket_write` | Write async data |
| `async_socket_cancel` | Cancel pending I/O |
| `async_socket_timeout` | Read with timeout |
| `async_accept_basic` | Accept incoming connection |
| `async_buffer_pool` | Buffer pool reduces allocs |
| `io_context_concurrent` | Multiple I/O ops simultaneously |
| `io_context_shutdown` | Graceful shutdown |
| `io_context_stress` | 10k connections |
| `io_context_cross_platform` | Works on Linux/Win/macOS |

#### Estimated Effort
- 7-10 days implementation
- 3 days testing per platform
- 2 days benchmarking

---

### Phase 5: HttpServer Refactor

#### Goals
- Replace `one-thread-per-connection` with event loop + thread pool
- Maintain backward compatibility (same `Server` API)
- Add `AsyncServer` variant for full async

#### Changes to `HttpServer.h`

```cpp
namespace OmniNet::HTTP {

class Server {
public:
    struct Config {
        // ... existing fields
        
        // New: enable async mode
        enum class Mode { Sync, Async, Hybrid };
        Mode mode = Mode::Hybrid;  // Default: use async I/O, sync handlers
        
        // New: thread pool config
        Thread::ThreadPool::Config threadPool = {};
    };
    
    // New: explicit thread pool injection
    Server(Config config, Thread::ThreadPool& pool);
    
    // Async run coroutine
    Async::Task<void> run_async();
    Async::Task<void> stop_async();
};

}
```

#### New: `AsyncServer` Class

```cpp
namespace OmniNet::HTTP {

class AsyncServer {
public:
    explicit AsyncServer(IO::IoContext& ctx, Config config = {});
    
    // Coroutine-based main loop
    Async::Task<void> run();
    Async::Task<void> stop();
    
    // Async route registration (same as Server)
    void get(std::string, AsyncHandler);
    void post(std::string, AsyncHandler);
    // ...
    
private:
    IO::IoContext& m_context;
    Router m_router;
    // ...
};

}
```

#### Migration Path

1. **v0.4.0**: Add `Mode::Hybrid` (default). Old `Server` works unchanged.
2. **v0.4.1**: Add `AsyncServer` for fully async use.
3. **v0.5.0**: Deprecate `Mode::Sync` (still works, emits warning).
4. **v0.6.0**: Remove `Mode::Sync` (sync handlers wrapped automatically).

#### Benchmark Targets

| Metric | Current | Target |
|--------|---------|--------|
| Connections (10k) | ❌ Fails | ✅ Works |
| Latency p99 | 5 ms | < 1 ms |
| Throughput | 5k req/s | 50k req/s |
| Memory per conn | 8 KB | 1 KB |
| Threads | 1 + N | N |

#### Estimated Effort
- 4-5 days implementation
- 2 days testing
- 1 day benchmarking

---

### Phase 6: Database & Other I/O Async

#### Async Database API

```cpp
namespace OmniNet::DB {

class AsyncDatabase {
public:
    explicit AsyncDatabase(IO::IoContext& ctx, std::string path);
    
    // Async query (returns future)
    Async::Task<std::vector<Row>> query(std::string_view sql);
    Async::Task<QueryResult> query_prepared(std::string_view sql);
    
    // Async execute
    Async::Task<void> execute(std::string_view sql);
    
    // Connection pool integration
    static AsyncDatabase& from_pool(DatabasePool& pool);
};

class DatabasePool {
public:
    // Acquire connection (blocking with timeout)
    std::future<std::shared_ptr<AsyncDatabase>> acquire(
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    
    // Async acquire
    Async::Task<std::shared_ptr<AsyncDatabase>> acquire_async();
    
    // Return to pool
    void release(std::shared_ptr<AsyncDatabase> db);
    
    // Pool config
    struct Config {
        size_t minSize = 2;
        size_t maxSize = 16;
        std::chrono::seconds idleTimeout{300};
    };
};

}
```

#### Other Async Components

| Component | Async API | Priority |
|-----------|-----------|----------|
| Database | `AsyncDatabase` + `DatabasePool` | High |
| Logger | `AsyncLogger` (off-thread writes) | Medium |
| File I/O | `AsyncFile` (kernel AIO / io_uring) | Medium |
| TLS | Async handshake via SSL_CTX_set_info_callback | High |
| Vector Store | `AsyncVectorStore` | Low |
| WebSocket | Already async-friendly | None |
| SSE | Already async-friendly | None |

#### Estimated Effort
- 3-4 days (Database)
- 2-3 days (Logger)
- 2-3 days (File I/O)
- 2-3 days (TLS)

---

### Phase 7: Integration & Polish

#### Gzip Compression Integration

```cpp
// In HttpConnection::sendResponse()
void Connection::sendResponse(Response& resp) {
    // Check if client accepts gzip
    auto acceptEncoding = m_currentRequest.header("Accept-Encoding");
    if (acceptEncoding && acceptEncoding->find("gzip") != std::string::npos) {
        if (resp.compressBody(*acceptEncoding)) {
            LOG_DEBUG("Compressed response: {} -> {} bytes",
                      originalSize, resp.bodySize());
        }
    }
    // ... existing send logic
}
```

#### Error Handling Modernization

Replace `bool` returns with `std::expected<T, Error>`:

```cpp
// Before
bool connect(const std::string& host, uint16_t port);

// After
std::expected<void, SocketError> connect(const std::string& host, uint16_t port);

enum class SocketError {
    InvalidAddress,
    ConnectionRefused,
    Timeout,
    NetworkUnreachable,
    // ...
};
```

#### Structured Logging

```cpp
// Use std::source_location for automatic location capture
#define LOG_INFO(msg) OmniNet::Logger::log(
    OmniNet::LogLevel::INFO, 
    msg, 
    std::source_location::current()
)
```

---

### Migration Compatibility Matrix

| Existing Code | v0.4.0 | v0.5.0 | v0.6.0 |
|---------------|--------|--------|--------|
| `Server::start()` | ✅ | ✅ | ✅ |
| `Server::get(pattern, Handler)` | ✅ | ✅ | ✅ (auto-wrap) |
| `Connection::processRequests()` | ✅ | ✅ | ✅ |
| `Socket::connect()` (returns void) | ✅ | ⚠️ Deprecate | ❌ |
| `Socket::connect() -> std::expected` | ✅ (new overload) | ✅ | ✅ |
| `Server::workerCount` | ✅ | ✅ | ✅ |
| `std::thread` usage in user code | ✅ | ✅ | ✅ |

---

### Phased Rollout

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| **1. C++23 Build** | 1 week | All tests pass on C++23 |
| **2. ThreadPool** | 1 week | `ThreadPool` class + tests |
| **3. Async Handlers** | 1.5 weeks | `Async::Task` + `AsyncHandler` |
| **4. Non-blocking I/O** | 2-3 weeks | `IoContext` + `AsyncSocket` |
| **5. HttpServer Refactor** | 1.5 weeks | Async-aware `Server` |
| **6. Async DB & I/O** | 2 weeks | `AsyncDatabase`, `DatabasePool` |
| **7. Polish & Release** | 1 week | Docs, examples, benchmarks |

**Total**: ~10-12 weeks (can be parallelized with multiple contributors)

---

### Files to Create

```
src/thread/ThreadPool.h
src/thread/ThreadPool.cpp
src/thread/WorkStealingQueue.h
src/thread/Task.h
src/thread/StopToken.h
src/async/Task.h
src/async/Promise.h
src/async/Awaitable.h
src/async/Executor.h
src/io/IoContext.h
src/io/EpollContext.h
src/io/KqueueContext.h
src/io/IocpContext.h
src/io/AsyncSocket.h
src/io/AsyncTimer.h
src/io/AsyncBuffer.h
src/db/AsyncDatabase.h
src/db/AsyncDatabase.cpp
src/db/DatabasePool.h
src/db/DatabasePool.cpp
include/OmniNet/ThreadPool.h
include/OmniNet/Async.h
include/OmniNet/IoContext.h
tests/test_thread_pool.cpp
tests/test_async_task.cpp
tests/test_io_context.cpp
tests/test_async_database.cpp
docs/threadpool.md
docs/async.md
docs/async-handlers.md
```

### Files to Modify

```
CMakeLists.txt                        (C++23, link new modules)
src/http/HttpServer.h                 (add Mode, AsyncServer)
src/http/HttpServer.cpp               (use ThreadPool/IoContext)
src/http/HttpConnection.cpp           (async I/O)
src/http/HttpRequest.cpp              (use std::expected)
src/http/HttpResponse.cpp             (integrate gzip)
src/http/Router.cpp                   (async handlers)
src/net/Socket.h                      (return std::expected)
src/net/Socket.cpp                    (return std::expected)
src/net/TLSSocket.h                   (async handshake)
src/net/TLSSocket.cpp                 (async handshake)
src/db/Database.cpp                   (async versions)
src/utils/Logger.cpp                  (async logging)
README.md                             (C++23 requirement)
docs/api/*.md                         (update for new APIs)
```

---

### Performance Targets

| Benchmark | Before | After | Improvement |
|-----------|--------|-------|-------------|
| Echo server, 1k connections | 5k req/s | 50k req/s | **10x** |
| Echo server, 10k connections | OOM/crash | 30k req/s | **Works** |
| p99 latency (echo) | 5 ms | 0.5 ms | **10x** |
| Memory per connection | 8 KB | 1 KB | **8x** |
| Threads required (10k conn) | 10,000 | 8 | **1250x** |

---

### Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| C++23 compiler availability | Build failures | Require GCC 13+ / Clang 17+; document |
| Coroutine ABI changes | Linking issues | Lock to one toolchain version per release |
| Backward compat breaks | User code fails | Phased rollout, deprecation warnings |
| Performance regression | Latency up | Benchmark suite in CI |
| Thread pool starvation | Deadlocks | Bounded queues, timeouts, metrics |
| Platform-specific bugs | Cross-platform issues | Test matrix: Linux, macOS, Windows |

---

### Open Questions

1. **Coroutine vs callback API**: Use C++20 coroutines as primary? Both?
2. **ThreadPool library**: Roll our own or use `boost::asio::thread_pool`?
3. **IoContext library**: Roll our own, use `boost::asio`, or use `libuv`?
4. **Async DB driver**: Roll our own, or use existing connection pooling?
5. **Backpressure strategy**: Block submitter, or reject with error?

### Decision Log

- [ ] **C++23 minimum**: TBD (may wait for GCC 14 LTS)
- [ ] **Coroutine support**: TBD (assume yes based on plan)
- [ ] **ThreadPool location**: `src/thread/` (not `src/concurrency/`)
- [ ] **IoContext location**: `src/io/`
- [ ] **Naming**: `Async::Task<T>` vs `Task<T>`

---

### References

- C++23 Features: https://en.cppreference.com/w/cpp/23
- C++20 Coroutines: https://en.cppreference.com/w/cpp/language/coroutines
- Boost.Asio: https://www.boost.org/doc/libs/release/libs/asio/
- io_uring: https://kernel.dk/io_uring.pdf
- Lock-free queues: https://www.1024cores.net/home/lock-free-algorithms/queues

---

**Last Updated**: 2026-05-23 15:30 AEST