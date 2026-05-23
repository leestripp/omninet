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
1. **Tools vs Library Duplication** - Separate database implementations in `tools/tools_common/` and `src/db/`
2. **Gzip Disabled** - zlib not found, compression not implemented

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

### In Progress 🔄
- None

### TODO 📋
- [x] **Phase 1**: Token Utilities (`src/token/TokenUtils.h/cpp`) ✅
- [x] **Phase 2**: Session Token Store (`src/token/SessionTokenStore.h/cpp`) ✅
- [x] **Phase 3**: API Key Store (`src/token/ApiKeyStore.h/cpp`) ✅
- [x] **Phase 4**: Unit tests for token system ✅ (28 tests)
- [x] **Documentation**: Updated README.md and created docs/api/token.md
- [ ] Fix SQL injection in SessionStore/UserStore (existing code)
- [ ] Consolidate tools/library database implementations
- [ ] Enable gzip compression (install zlib)

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
| 2026-05-23 | 0.3.0 | Current version from git tag |
| 2026-05-23 | - | Implemented: Token System (TokenUtils, SessionTokenStore, ApiKeyStore) |
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

**Last Updated**: 2026-05-23 11:30 AEST