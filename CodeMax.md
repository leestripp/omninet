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

## v0.5.0 Plan: Integration & Demos

**Status**: 📋 Planning
**Target Version**: 0.5.0
**Estimated Effort**: 2-3 weeks

### Goal

After v0.4.0 (infrastructure) and v0.4.1 (async extensions), the codebase has:
- ✅ C++23 build, ThreadPool, Async Tasks, I/O, Server
- ✅ AsyncLogger, AsyncTlsSocket, AsyncFile, AsyncDatabase
- ✅ std::expected<T, E> migration
- ✅ 265 tests across 12 test suites

What's missing for a complete v0.5.0:
- **Real-world demo** using all the new async features
- **Performance benchmarks** to validate gains
- **End-to-end documentation** showing patterns

---

### Phase 9a: Async Server Integration Demo

Build a complete demo application showcasing all v0.4.0+ features.

**Files**:
```
demos/async_server_demo/
├── main.cpp           # Server setup with all features
├── routes.cpp         # Example route handlers
├── routes.h           # Route declarations
├── CMakeLists.txt     # Build configuration
└── README.md          # Usage instructions
```

**Features demonstrated**:
- AsyncServer with coroutine handlers
- AsyncDatabase for queries (no blocking)
- AsyncLogger for non-blocking logs
- AsyncFile for static file serving
- std::expected<T, E> error handling
- Connection pool for database
- TLS support via AsyncTlsSocket
- Gzip compression (auto)

#### Estimated Effort: 3-4 days

---

### Phase 9b: Performance Benchmarks

Validate the performance claims from the refactor plan:
- 10x throughput vs old one-thread-per-connection
- 8KB → 1KB memory per connection
- 1M+ log messages/sec (AsyncLogger)
- Sub-millisecond error handling (std::expected)

**Files**:
```
benchmarks/
├── echo_server_bench.cpp        # Throughput benchmark
├── connection_scaling_bench.cpp # Connection scaling
├── logger_throughput_bench.cpp  # AsyncLogger benchmark
├── expected_overhead_bench.cpp  # Error handling overhead
└── CMakeLists.txt
```

**Targets** (from plan):
| Metric | v0.4.0 | v0.5.0 Target |
|--------|--------|---------------|
| Echo server 1k conn | 5k req/s | 50k req/s |
| Echo server 10k conn | OOM | 30k req/s |
| p99 latency | 5ms | 0.5ms |
| Memory per conn | 8KB | 1KB |
| Log throughput | 10k msg/s | 1M msg/s |

#### Estimated Effort: 2-3 days

---

### Phase 9c: Documentation Update

Finalize documentation for all new modules:
- `docs/api/threadpool.md` - ThreadPool usage
- `docs/api/async.md` - Async::Task and coroutines
- `docs/api/io.md` - IoContext, AsyncSocket, AsyncFile
- `docs/api/async-server.md` - AsyncServer usage
- `docs/api/async-database.md` - DatabasePool, AsyncDatabase
- `docs/api/async-logger.md` - AsyncLogger
- `docs/api/async-tls.md` - AsyncTlsSocket
- `docs/api/expected.md` - std::expected migration guide

**Migration Guide** (most important):
- `docs/migration/v0.4-to-v0.5.md` - Step-by-step upgrade guide
- Before/after code examples
- Compatibility matrix

#### Estimated Effort: 2-3 days

---

### Phased Rollout

| Phase | Duration | Deliverable | Status |
|-------|----------|-------------|--------|
| **9a. Integration Demo** | 3-4 days | Complete demo using all features | ✅ Done |
| **9b. Benchmarks** | 2-3 days | Performance validation | ✅ Done |
| **9c. Documentation** | 2-3 days | API docs + migration guide | ✅ Done |
| **Release 0.5.0** | 1 day | Tag, demos, benchmark results | ✅ Done |

**Total**: ~2-3 weeks

---

### Open Questions

- [ ] **Demo scope** - single comprehensive demo or per-feature demos?
- [ ] **Benchmark methodology** - include in CI or separate?
- [ ] **Documentation format** - markdown, doxygen, or both?

---

**Last Updated**: 2026-06-11 12:35 AEST## Architecture Summary

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
### In Progress 🔄
- [x] **prompt.txt Bug #5: WebSocket post-upgrade handler race** ✅ FIXED in `0c4f471`
  - **Problem**: WebSocket::Server::on() called m_httpServer->any() then
    m_httpServer->setPostUpgradeHandler() — a TOCTOU race. When N clients
    connect in ~1ms, ~1 of N got a working WebSocket; N-1 got 404.
    Additionally, m_postUpgradeHandlers.erase(it) made the registration
    single-shot, breaking multi-client even without the race.
  - **Fix** (commit `0c4f471`):
    1. Added `ProtocolSwitchHandlers` struct + `registerProtocolSwitch()`
       in HttpServer — one call, one lock, atomic registration.
    2. Connection loop checks `m_protocolSwitches` FIRST (before Router).
       If protocol-switch handler found, invoke `on_upgrade` directly.
       Only call Router if NOT a protocol switch (prevents 404 from
       overriding 101 when Router has no /echo route).
    3. Post-upgrade callback is persistent (no erase) — multi-client works.
    4. WebSocket::Server::on() uses `registerProtocolSwitch` (one call).
  - **Tests**: test_websocket_broadcast 10/10 PASS, test_sync_http_server
    18/18 PASS, no regressions. test_websocket_echo still hangs in
    sleep_for(500ms) (pre-existing starvation issue, unrelated).

### TODO 📋

### TODO 📋
- [ ] **dashboard_demo POST /api/dashboard/login times out** (v0.5.2 known limitation) ✅ FIXED in v0.6.0 (root cause was `Response::build()` not auto-setting `Content-Length`, fixed in `src/http/HttpResponse.cpp`)
  - `POST /api/dashboard/login -d "username=admin&password=test"` hangs in curl
  - POST works fine in `http_server_demo`'s `/api/echo`, so this is specific
    to the dashboard login flow, not the sync HTTP server
  - Likely cause: session/DB init issue in the demo, or the `UserStore::verify`
    is blocking on something
  - All other dashboard endpoints work (GET /api/dashboard/stats, /users, etc.)
  - To debug: enable DEBUG logging and trace the request, or add cerr in
    `DashboardHandler::handleLogin` to see how far it gets
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
### In Progress 🔄
### In Progress 🔄
- [x] **prompt.txt (ApolloCore integration 2026-06-27) — triage + 1 real fix** ✅ DONE
  - Triage of 13 reported bugs. Most were either:
    - **NOT OmniNet bugs** (#1 SQLiteDB::step is ApolloCore; #9-13
      are llama.cpp / ApolloCore). Skipped.
    - **ALREADY FIXED** in this tree: #3 `setBody(string_view)` at
      HttpResponse.h:138, #5 `setStatus(int)` at HttpResponse.h:102.
    - **NOT A BUG** (working as intended): #2 `delete_()` (C++ keyword
      constraint, trailing underscore is required), #6 "no shutdown"
      (`Server::stop()` exists at HttpServer.h:94 — doc gap at most).
    - Vague / out of scope: #7, #8.
  - **Real fix applied** (commit `ba9b2cf`): Bug #4 — added typed
    accessors `queryInt(key)`, `queryInt(key, default)`,
    `queryDouble(key)`, `queryDouble(key, default)` to `QueryParams`.
    Saves users from `std::string(view)` boilerplate before
    `std::stoi`/`std::stod`. Trailing non-digit chars rejected.
  - **Tests**: test_omnet_test 110/110 PASS, build clean.

### TODO 📋
    This crashed with `*** bit out of range 0 - FD_SETSIZE on
    fd_set ***` because `FD_SET(INVALID_SOCKET, ...)` writes
    out of bounds in `TLSSocket::recvAll`.
  - **Fix**: Add `m_socket->connect(parsed->host, parsed->port)`
    after the `setALPNProtocols` call, mirroring what
    `createConnectedSocket()` does for the plain `ws://` branch.

### TODO 📋
    `parsed->secure`. **Fix** (commit `8cdf9e3`): branch on
    `parsed->secure` — for `ws://` use `createConnectedSocket()`
    (existing); for `wss://` use
    `TLSContext::createClient()->createSocket()` with SNI from
    `parsed->host` and ALPN `{"http/1.1"}`. Previously wss://
    silently sent a plaintext upgrade, causing opaque handshake
    failures.
  - **Tests**: build clean, no regressions.

### TODO 📋
    socket's `m_ctx` aliases the context's `m_ctx` — both destructors
    then free the same pointer.
  - **Fix**: Track ownership via `m_context` (the shared_ptr). In
    `cleanupContext()`, only call `SSL_CTX_free(m_ctx)` when
    `m_context` is null (i.e., the socket owns its own context).
    When `m_context` is non-null, the shared `TLSContext` owns
    the SSL_CTX and is the sole owner responsible for freeing it.
  - **Tests**: build clean, no regressions.

### TODO 📋
    Previously, calling `readLine()` on a TLSSocket returned raw
    TLS ciphertext (the record header bytes), causing "malformed
    status line" errors on every HTTPS request.
  - **Bug 4** 🔴 HIGH: Default `initContext(CLIENT)` didn't load
    system CA bundle or set `SSL_VERIFY_PEER`. **Fix** (commit
    `fc97be9`): in `initContext()`, for client contexts, call
    `SSL_CTX_set_default_verify_paths()` to load the system CA
    bundle and `SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER, nullptr)`
    to require peer verification. Callers can still override via
    `setVerifyMode(NONE)` for dev/testing.
  - **Bug 1** 🟡 MED: `TLSContext::createClient()` and
    `createSocket()` declared but not defined. **Fix** (commit
    `913e44b`): made `TLSContext` inherit
    `std::enable_shared_from_this`; implemented `createServer()`,
    `createClient()`, `createSocket()`, and the private ctor +
    dtor. Added `TLSSocket(shared_ptr<TLSContext>)` constructor
    and `setContext()` helper to adopt a shared context.
  - **Bug 2** 🟡 MED: `setALPNProtocols()` was a no-op. **Fix**
    (commit `d508494`): build the ALPN wire format (RFC 7301:
    length-prefixed protocol names) and call
    `SSL_CTX_set_alpn_protos()` on `m_ctx`. Without this, modern
    HTTPS servers default to HTTP/2 which has a binary wire format
    incompatible with text-line HTTP/1.1 parsers.
  - **Bug 5** 🟢 LOW: Fixed as part of Bug 1 (TLSContext now
    inherits `std::enable_shared_from_this`).
  - **Tests**: test_omnet_test 110/110 PASS, full build clean.
    tool from doing HTTPS without workarounds. Priority order from prompt:
  - **Bug 3** 🔴 HIGH: `TLSSocket::readLine()` not overridden → silent data
    corruption on TLS. Plan: override `readLine()` to use the overridden
    `recv()` (which uses `SSL_read`), not raw `::recv()`.
  - **Bug 4** 🔴 HIGH: Default `initContext(CLIENT)` doesn't load system
    CA bundle or set `SSL_VERIFY_PEER`. Security footgun. Plan: call
    `SSL_CTX_set_default_verify_paths()` and set verify mode to
    `SSL_VERIFY_PEER` in client mode.
  - **Bug 1** 🟡 MED: `TLSContext::createClient()` and `createSocket()`
    declared but not defined → link fails. Plan: implement both in
    `TLSSocket.cpp`; make `TLSContext` inherit `std::enable_shared_from_this`
    so `createSocket()` can use `shared_from_this()`.
  - **Bug 2** 🟡 MED: `setALPNProtocols()` is a no-op. Plan: actually
    call `SSL_CTX_set_alpn_protos` on `m_ctx` (or `SSL_set_alpn_protos`
    on `m_ssl` after handshake). Also wire up `getALPNProtocol()` to
    call `SSL_get0_alpn_selected(m_ssl, ...)` after handshake.
  - **Bug 5** 🟢 LOW: `createServer()` shared_ptr quirk. Plan: make
    `TLSContext` inherit `std::enable_shared_from_this` (part of Bug 1 fix).

### TODO 📋
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
| 2026-06-11 | **0.5.2** | Demo fixes: HTTPS (certs) + dashboard (route ordering) |
| 2026-06-11 | 0.5.1 | AsyncServer + Task<T> bug fixes (see below) |
| 2026-05-23 | 0.5.0 | Integration & Demos (3 phases complete) |
| 2026-05-23 | 0.4.1 | Async I/O Extensions (4 phases complete) |
| 2026-05-23 | 0.4.0 | C++23 + Async + ThreadPool refactor (7 phases) |
| 2026-05-23 | 0.3.2 | Enable zlib/gzip compression |
| 2026-05-23 | 0.3.1 | Token system: SessionTokenStore + ApiKeyStore |
| 2026-05-23 | 0.3.0 | Current version from git tag |
| 2026-05-22 | - | Implemented sqlite-vec FetchContent build |
| 2026-05-22 | - | Simplified vector plan (embeddings out of scope) |
| 2026-05-15 | - | Fixed SQLite CMake target name |
| 2026-05-04 | - | Added Cppcheck static analysis |
| 2026-04-18 | - | Initial HTTP/WebSocket implementation |

## v0.5.2: Demo Fixes (2026-06-11)

**Found while testing all 6 demos end-to-end.**

### `https_server_demo` — fixed by generating certs

The HTTPS demo was reading TLS ClientHello data as plain HTTP, causing
the connection to hang. Root cause:

1. `~/.omninet/config.json` had `"tls": {"enabled": false, "certFile": "", "keyFile": ""}`
2. `~/.omninet/certs/` didn't exist
3. Demo's `serverConfig.tlsEnabled` read `false` from config, so the
   server bound the port but used plain HTTP
4. Curl sent TLS data, server read it as HTTP, got stuck

**Fix**: Run `omninet-init --enable-tls -y --skip-admin` which:
- Generates `~/.omninet/certs/omninet.crt` and `~/.omninet/certs/omninet.key`
- Sets `"tls.enabled": true` and points certFile/keyFile to the right paths
- Re-init with `--force` if `~/.omninet/` already exists

All HTTPS endpoints now work.

### `dashboard_demo` — fixed route ordering

The `/*` catch-all static-file route was registered BEFORE `/api/status`,
so the catch-all always matched first and returned 404 from the static
handler (no file at `/api/status`).

**Fix** (same pattern as `http_server_demo`):
1. `dashboard.mount(server)` — specific `/dashboard`, `/api/dashboard/*`
2. `server.router().get("/api/status", ...)` — specific JSON endpoint
3. `server.router().get("/", staticHandler)` — serves `index.html`
4. `server.router().get("/*", staticHandler)` — catch-all LAST, with
   a check for reserved prefixes (`/api/`, `/dashboard`, `/omninet/`)
   so it returns 404 instead of falling through to the static handler

All dashboard endpoints now work.

### Known limitation

`POST /api/dashboard/login` still times out in the dashboard demo. POST
works fine in `http_server_demo`'s `/api/echo`, so this is specific to
the dashboard login flow (likely a session/DB init issue in the demo
itself, not the sync HTTP server).

## v0.5.1: AsyncServer Bug Fixes (2026-06-11)

**Discovered**: Testing the async_server_demo revealed 5 critical bugs.

### Critical fixes

1. **AsyncServer was discarding async handlers** (`src/server/AsyncServer.cpp`)
   - The `get()/post()/etc.` methods registered a no-op sync handler and
     threw away the `AsyncHandler` (literally marked with `TODO`).
   - **Fix**: Handlers are now stored in `m_asyncRoutes` and dispatched
     to on each request.

2. **AsyncServer accept loop was a no-op**
   - `accept_loop()` did `co_return;` immediately (marked `TODO`).
   - **Fix**: An async `accept_loop()` coroutine now watches the listen
     socket via `IoContext::watch()` and spawns a `handle_connection()`
     coroutine per accept.

3. **Task<T>::await_suspend returned `false`** (`src/async/Task.h`)
   - Awaiting a coroutine that used `co_await` internally silently failed.
     The continuation was set but never resumed.
   - **Fix**: `final_suspend` now resumes the continuation (symmetric
     coroutine pattern), `await_suspend` returns `void` (always suspend).
   - This was the root cause: the AsyncServer's `co_await handlerTask`
     never woke up.

4. **`async_server_demo` seeded a separate in-memory DB**
   - The handlers queried the pool's DB (empty), so queries returned
     "no such table: users".
   - **Fix**: Seed via a connection from the pool; use a file-backed DB
     (`/tmp/async_server_demo.db`) since SQLite in-memory DBs are
     per-connection and don't share data.

5. **`async_server_demo` seed code didn't await `execute()`**
   - `execute()` is a coroutine; the seed code created a `Task` and
     immediately destroyed it without running.
   - **Fix**: Use `OmniNet::Async::blocking_get()` to wait for each.

### AsyncServer features added

- Async `accept_loop()` watching the listen socket
- `handle_connection()` using `AsyncSocket` for non-blocking I/O
- Path parameter matching (`:id` → `([^/]+)`)
- Wildcard matching (`*` → `.*`)
- HTTP/1.1 keep-alive support (loop while `req.keepAlive()`)
- Use `Response::buildFull()` (headers + body) instead of `build()`

### Test results

- AsyncServer tests: **6 → 12** (added 6: dispatch, 404, :id, *, chained
  coroutine, stats; all use real HTTP requests over a raw socket)
- Total tests: **213 → 271** (all passing)
- All 8 async_server_demo endpoints verified working

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
| Tests | ✅ 271/271 passing |
| Library | ✅ OmniNet static library |
| Demos | ✅ 5 demos (all working in v0.5.2) |
| Tools | ✅ 10 CLI tools |
| Vector | ✅ sqlite-vec integration |
| Static Analysis | ✅ cppcheck targets |
| Token System | ✅ Implemented |
| Compression | ✅ zlib enabled, integrated |
| C++23 Build | ✅ Upgraded |
| ThreadPool | ✅ Work-stealing pool |
| Async Tasks | ✅ C++20 coroutines |
| Async I/O | ✅ epoll-based |
| AsyncServer | ✅ Working (12 tests, v0.5.1 fix) |
| AsyncDatabase | ✅ Connection pool |

**Last Updated**: 2026-06-11 12:35 AEST

---

## Future Work (Potential)

| Feature | Description | Priority |
|---------|-------------|----------|
| ~~Gzip response compression~~ | ~~Implement in HttpResponse for `Accept-Encoding: gzip`~~ ✅ Done | Medium |
| ~~C++23 + Async + ThreadPool Refactor~~ | All 7 phases complete | **Done** |
| `std::expected<T, E>` error handling | Modernize Socket/DB return types | Low |
| AsyncLogger | Off-thread log writes via ThreadPool | Low |
| HTTP/2 support | Upgrade path for performance | Low |
| Rate limiting middleware | Global rate limiter with Redis support | Medium |
| WebSocket load balancing | Session affinity or stateless mode | Low |
| HTTP/2 support | Upgrade path for performance | Low |
| Metrics export | Prometheus/Graphite integration | Low |

---

## v0.6.0 Plan: Bug Fixes, Portability & Quality

**Status**: 📋 Planning
**Target Version**: 0.6.0
**Estimated Effort**: 4-6 weeks (can be parallelized)
**Discovered**: 2026-06-11 bug/issue sweep

### Executive Summary

A comprehensive sweep of the codebase found:
- **3 real bugs** (TODO comments in `src/`)
- **1 known limitation** (dashboard demo POST timeout, already in TODO)
- **2 missing platform implementations** (KqueueContext, IocpContext — only Linux/epoll works)
- **1 missing public API** (`include/OmniNet/` has only `Test.h`; the plan in CodeMax.md
  says expose ThreadPool/Async/IoContext, none of which exist there)
- **7 test coverage gaps** (chunked transfer, compression, WebSocket protocol masking,
  large frames, sync HTTP::Server, async TLS handshake, demo integration)
- **1 broken tool** (`omninet-certs generate` writes to cwd, not `~/.omninet/certs/`)
- **2 performance TODOs** (edge-triggered epoll, truly non-blocking AsyncDatabase)

### Phase 10a: Critical Bug Fixes (3-4 days)

**Status: ✅ COMPLETE (2026-06-11)**

| # | File | Issue | Fix | Tests |
|---|------|-------|-----|-------|
| 1 | `src/http/HttpConnection.cpp` | Chunked transfer-encoding body is **discarded** — the code parses chunk size and extracts `chunkData` but never accumulates it into `m_currentRequest`. Only the final 0-chunk sets the (incorrect) body. | Accumulate `chunkData` into a local `chunkedBody`, process ALL chunks in the buffer in a loop (not just one per recv), set the accumulated body on the final 0-chunk. | Added `tests/test_http_connection.cpp` with 5 tests (single chunk, multiple chunks, regression, GET, invalid size) |
| 2 | `src/db/Database.cpp:233` | All `sqlite3_open` errors mapped to `DbError::Unknown` | Switch on the return code and map to specific DbError variants (CANTOPEN→InvalidPath, PERM→SystemError, NOMEM→OutOfMemory, READONLY→ReadOnly, CORRUPT/NOTADB→CorruptDatabase, BUSY→Busy). Also log the error message via cerr for debugging. | Added 2 tests in `tests/test_expected.cpp` (invalid_path, error_messages) |
| 3 | `src/http/HttpResponse.cpp` (root cause) | `Response::build()` did not auto-add `Content-Length`. For empty-body responses (e.g. 401 from dashboard login), HTTP/1.1 keep-alive clients (curl) hung waiting for data that never came. | Auto-add `Content-Length: N` based on body size when not explicitly set and no Transfer-Encoding / file body. Explicit setContentLength() calls are preserved. | Added 3 tests in `tests/test_http_response.cpp` (empty body, with body, explicit). Dashboard demo POST endpoints now work. |
| 4 | `tools/omninet-certs/main.cpp` | `generate` wrote certs to cwd (`omninet.crt` / `omninet.key`) instead of `Paths::certsDir()`. Made certs invisible to demos that look in `~/.omninet/certs/`. | Use `Paths::certsDir() / (opts.target + ".crt")` and create dir if missing. Matches the other actions (list, info, check). | Manual test: `omninet-certs generate localhost` now writes to `~/.omninet/certs/localhost.crt`; `list` shows it. |

**Result**: 271 → 275 tests passing (+4). All 4 critical bugs from the v0.6.0 sweep fixed.

### Phase 10b: Async TLS Completion (3-4 days)

**Status: ✅ PARTIAL (2026-06-11) — handshake now truly async; cancel() still TODO**

| Task | Status | Notes |
|------|--------|-------|
| Make `AsyncTlsSocket::handshake()` truly async | ✅ Done | Loops calling `stepHandshake()`, `co_await when_readable/when_writable` on WantRead/WantWrite. No more blocking in select(). |
| Implement `AsyncTlsSocket::cancel()` | 🔴 TODO | Currently a no-op. Proper cancellation needs a `weak_ptr`-based mechanism in the awaiter (the awaiter's callback captures a coroutine handle; if the handle is destroyed, resuming it is UB). The cleanest design: have AsyncTlsSocket track a "cancelled" flag, the awaiter checks the flag in its callback, and uses a `weak_ptr<Task<>>` to safely skip resume. |
| Add `test_async_tls_handshake` (real loopback) | ⚠️ Skipped for now | Would need cert files. The 2 new tests (`tls_socket_handshake_state_enum`, `init_handshake_idempotent`) verify the new stepHandshake() API and WantRead state. Full handshake test pending when test certs are wired up. |
| Add `test_async_tls_read_write` | ⚠️ Skipped for now | Same - pending cert infrastructure. |

### Phase 10c: Portability (1-2 weeks)

**Currently Linux-only. Docs claim cross-platform.**

| Task | Effort | Notes |
|------|--------|-------|
| Implement `KqueueContext` | 3-4 days | Mirror `EpollIoContext` API; use `EVFILT_READ`/`EVFILT_WRITE` |
| Implement `IocpContext` (Windows) | 1-2 weeks | Use `CreateIoCompletionPort` + `GetQueuedCompletionStatusEx`. New platform, needs Windows build infra. |
| Add macOS to CI | 1 day | `KqueueContext` smoke test in CI |
| Document `AsyncServer` portability | 1 hour | Currently Linux-only by virtue of `accept4(SOCK_NONBLOCK)`; needs `O_NONBLOCK` + `fcntl` on macOS/BSD |

### Phase 10d: Test Coverage Gaps (1 week)

**Currently 281 tests. Several important code paths have no coverage.**

| Test | What it covers | Status |
|------|----------------|--------|
| `test_chunked_transfer` | `HttpConnection` chunked body parsing | ✅ Phase 10a — already exists |
| `test_compression` | `Response::compressBody` + Accept-Encoding negotiation | 🔲 Missing |
| `test_websocket_protocol` | Real handshake, framing, masking (client→server must mask), large frames (>125, >65535 bytes, 7-bit vs 8-byte length), fragmentation reassembly, ping/pong, close | 🔲 Missing |
| `test_sync_http_server` | Real HTTP/1.1 request/response via raw socket (no async) | ✅ 9 tests: start/stop, GET, POST, 404, 405, multiple routes, keep-alive, Content-Length, multi-worker |

**Phase 10d sub-tasks completed:** `test_sync_http_server` (9 tests)

Bug fixes discovered during Phase 10d:
- `HttpServer::stop()`: `shutdown(SHUT_RDWR)` on server socket before `close()` — `close()` alone cannot interrupt a blocking `accept()` on Linux
- `HttpServer::runAcceptLoop()`: catch `SocketException` from `accept()` and exit gracefully on stop
- `HttpServer::handleConnection()`: wrap detached thread lambda in try-catch to prevent `std::terminate()` on uncaught exceptions
- `Socket`: add `shutdown(int how)` method
| `test_async_tls_*` | Real handshake + read/write (see Phase 10b) | |
| `test_demo_integration` | Boot each demo, curl key endpoints, verify response | Catches demo regressions like the v0.5.2 cert/route-ordering bugs |

Target: **281 → ~330 tests**.

### Phase 10e: Public API (2-3 days)

**`include/OmniNet/` is essentially empty (only `Test.h`).**

The plan in CodeMax.md (`docs/migration/v0.4-to-v0.5.md` was planned but never written) says the public API should expose:
- `OmniNet/ThreadPool.h`
- `OmniNet/Async.h` (umbrella for Task, Executor, AsyncHandler)
- `OmniNet/IoContext.h`
- `OmniNet/AsyncServer.h`
- `OmniNet/HttpRequest.h`, `HttpResponse.h`, `Router.h`
- `OmniNet/Expected.h`
- `OmniNet/Crypto.h`

Add a top-level `OmniNet.h` that includes the public surface. Mirror the structure that demos use today (`#include "async/Task.h"`) into a stable public API.

### Phase 10f: Truly Non-Blocking AsyncDatabase (2-3 days)

`AsyncDatabase::execute/query/query_one` submit to executor then call `fut.get()` (blocking) inside a coroutine. The coroutine looks async but actually blocks the calling thread. For the `AsyncServer` connection handler running in the event loop, this means **the event loop blocks during DB queries**.

Options:
1. Use a dedicated DB executor (separate from the event loop's executor) — partial fix, just moves the block
2. Use `io_uring` for true async I/O (Linux 5.6+) — real fix but adds platform dependency
3. Use a separate thread pool for DB only, ensure it's NOT the same pool the connection handler uses

The right answer is #2 long-term; #3 is a reasonable near-term improvement.

### Phase 10g: Code Quality (3-5 days)

| Task | Notes |
|------|-------|
| Split `ApiKeyStore.cpp` (550 lines) | Extract token hashing helpers, key encoding, SQL into separate files |
| Split `SessionTokenStore.cpp` (466 lines) | Same as above |
| Remove `unusedFunction` warnings in `tools/tools_common/` | Either use them or remove them. cppcheck currently shows ~50 unused functions; many are real candidates for deletion. |
| Add error handling to `dashboard_demo::main` | The server thread is `.join()`ed but exceptions thrown in the thread are silently swallowed. |
| Add a "lint" CI check that fails the build on new TODOs in `src/` | Prevents future TODO accumulation. |

### Phase 10h: Edge-Triggered epoll (Optional, 2-3 days)

`EpollIoContext` uses level-triggered mode (`EPOLLLT`). The TODO in the original plan mentions edge-triggered (`EPOLLET`) for higher performance but it's more complex (must drain the fd in a loop on each event).

Not blocking. Only pursue if benchmarks show LT is the bottleneck.

### Phase 10i: Documentation (1-2 days)

| Task | Notes |
|------|-------|
| Create `CHANGELOG.md` | Auto-generate from git tags: 0.1.0, 0.1.5, 0.2.0, 0.3.0, 0.3.2, 0.4.0, 0.4.1, 0.5.0, 0.5.1, 0.5.2 |
| Write `docs/migration/v0.5-to-v0.6.md` | The plan was written but never executed |
| Update `docs/api/async.md` | Mentions `AsyncServer` but didn't document the v0.5.1 route-dispatch fix |
| Update `docs/api/io.md` | Mentions `KqueueContext`/`IocpContext` which don't exist (misleading) |
| Verify `README.md` links | |

### Phased Rollout

| Phase | Duration | Deliverable | Priority |
|-------|----------|-------------|----------|
| **10a. Critical bugs** | 3-4 days | Chunked body, SQLite errors, omninet-certs, dashboard POST | 🔴 Critical |
| **10b. Async TLS** | 3-4 days | Real handshake, cancel, tests | 🟠 High |
| **10d. Test gaps** | 1 week | Chunked, compression, WS protocol, sync server, demo integration | 🟡 Partial (10d: test_sync_http_server done ✅) |
| **10g. Code quality** | 3-5 days | Split large files, remove unused functions | 🟡 Medium |
| **10i. Documentation** | 1-2 days | CHANGELOG, migration guide, doc accuracy | 🟡 Medium |
| **10e. Public API** | 2-3 days | `include/OmniNet/` headers, umbrella include | 🟡 Medium |
| **10f. Non-blocking DB** | 2-3 days | Move DB work off the event loop | 🟢 Low |
| **10c. Portability** | 1-2 weeks | KqueueContext + (optionally) IocpContext | 🟢 Low |
| **10h. Edge-triggered epoll** | 2-3 days | EPOLLET mode + benchmark | ⚪ Skip unless benchmark demands |

**Total**: ~5 weeks of focused work, or ~8 weeks with Phase 10c.

### Open Questions

- [ ] **Chunked body fix scope**: Just fix the bug, or also add streaming body support to AsyncServer? (Streaming is a much larger change.)
- [ ] **KqueueContext priority**: Is macOS support actually needed? If no users are asking, defer to 0.7+.
- [ ] **Public API freeze**: Once 0.6.0 ships, is the public API stable, or are we OK with breaking changes in 0.7?
- [ ] **Dashboard demo fate**: Keep it as-is (with the POST bug), deprecate it, or fix it? It's the most complex demo and the most fragile.
- [ ] **Windows support**: Any actual demand? The Windows IOCP work is 1-2 weeks; defer if no users.

### Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Chunked body fix breaks existing chunked traffic | High | Add test before fixing; add regression test for clients that rely on the buggy behavior (none expected) |
| Public API move breaks downstream users | Medium | Keep `src/` headers working; add `include/OmniNet/` as parallel API; deprecate `src/` in 0.7 |
| KqueueContext implementation diverges from EpollContext | Low | Mirror API exactly; add cross-platform test |
| Test coverage additions uncover more bugs | High | Budget extra time; address bugs as they're found |

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

**Status**: ✅ Completed (v0.4.0)

#### What was delivered

1. **Gzip compression integration** — `HttpConnection::sendResponse()` now checks
   the request's `Accept-Encoding` header and automatically calls
   `Response::compressBody()` when gzip is supported. This is opt-in via the
   zlib detection at build time (the `OMNINET_HAVE_ZLIB` define).

2. **Backward compatibility** — All existing API continues to work. The
   compression is transparent to handlers.

3. **Documentation** — README.md updated with new module links.

#### What was deferred

- **Error handling modernization** — `std::expected<T, E>` migration is deferred
  to v0.4.1 to keep this release focused on infrastructure.
- **AsyncLogger** — Deferred to v0.4.1 as it requires integration with the
  new ThreadPool for off-thread writes.
- **AsyncFile** — Deferred; current synchronous file I/O is sufficient for
  static file serving.
- **TLS async handshake** — Deferred; TLS still happens synchronously in
  the existing `Server`.

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

## v0.4.1 Plan: Async I/O Extensions

**Status**: 📋 Planning  
**Target Version**: 0.4.1  
**Estimated Effort**: 3-4 weeks (incremental, backward-compatible)

### Goal

Complete the async migration started in v0.4.0 by adding:
- **AsyncLogger** — eliminate disk I/O blocking in hot paths
- **TLS async handshake** — enable secure AsyncServer
- **`std::expected<T, E>`** — modernize error handling, no exceptions
- **AsyncFile** — async static file serving (io_uring / thread pool)

All changes are **backward-compatible**. Existing sync code continues to work.

---

### Phase 8a: AsyncLogger

#### Problem

Current `Logger::log()` writes synchronously to disk on the calling thread:

```cpp
// Synchronous write — blocks if disk is slow
void Logger::log(LogLevel level, const std::string& message) {
    // ... format message ...
    if (m_logFile.is_open()) {
        m_logFile << formatted << std::endl;  // BLOCKS!
    }
}
```

For high-throughput servers, this becomes a bottleneck.

#### Solution

`AsyncLogger` queues log messages and writes them on a background thread:

```cpp
namespace OmniNet::Async {

class AsyncLogger {
public:
    explicit AsyncLogger(Thread::ThreadPool& pool);
    
    // Non-blocking log (queues for background write)
    void log(LogLevel level, std::string_view message);
    
    // Async log (returns immediately, message is owned)
    void log(LogLevel level, std::string message);
    
    // Flush pending messages (blocking)
    void flush();
    
    // Shutdown (flushes and stops)
    void shutdown();
    
    // Statistics
    struct Stats {
        uint64_t messagesQueued;
        uint64_t messagesWritten;
        uint64_t messagesDropped;  // if queue full
    };
    Stats stats() const;
};

}
```

#### Architecture

```
┌─────────────────┐
│  Logger::log()  │ (any thread)
└────────┬────────┘
         │ non-blocking
         ▼
┌─────────────────┐
│  MPSC Queue     │ (lockfree)
│  (messages)     │
└────────┬────────┘
         │ background
         ▼
┌─────────────────┐
│  Writer Thread  │ (ThreadPool worker)
│  flush to disk  │
└─────────────────┘
```

#### Files

```
src/async/AsyncLogger.h
src/async/AsyncLogger.cpp
tests/test_async_logger.cpp
docs/api/async-logger.md
```

#### Tests (~8 test cases)

- basic_log, non_blocking_log
- concurrent_log (1000 messages from 10 threads)
- flush_drains_queue
- shutdown_blocks_until_drained
- queue_full_drops_messages
- level_filtering
- rotation_async
- stats_tracking

#### Estimated Effort
- 2-3 days implementation + tests

---

### Phase 8b: TLS Async Handshake

#### Problem

Current `TLSSocket::performHandshake()` blocks the calling thread:

```cpp
// Server-side: blocking TLS handshake (can take 10-100ms)
tlsSocket->performHandshake();
// ... while this runs, the thread is blocked
```

For AsyncServer, we need non-blocking handshake.

#### Solution

Wrap TLS operations in coroutine-aware awaiters:

```cpp
namespace OmniNet::Async {

class AsyncTlsSocket {
public:
    AsyncTlsSocket(IO::IoContext& ctx, std::unique_ptr<TLSSocket> socket);
    
    // Async handshake (suspends until complete)
    Async::Task<> handshake();
    
    // Async read/write like AsyncSocket
    Async::Task<size_t> read_some(std::span<uint8_t> buf);
    Async::Task<size_t> write_some(std::span<const uint8_t> data);
    Async::Task<void> write_all(std::span<const uint8_t> data);
    
    // Cancel pending operations
    void cancel();
    
    // Native access
    TLSSocket& native() { return *m_socket; }
    
private:
    IO::IoContext& m_context;
    std::unique_ptr<TLSSocket> m_socket;
};

}
```

#### Implementation Strategy

- Use OpenSSL's non-blocking BIO
- Hook into `IoContext::watch()` for read/write readiness
- Re-handshake on `EAGAIN` / `SSL_ERROR_WANT_READ/WRITE`
- Same pattern as `AsyncSocket::read_some` retry loop

#### Files

```
src/async/AsyncTlsSocket.h
src/async/AsyncTlsSocket.cpp
tests/test_async_tls.cpp
```

#### Tests (~6 test cases)

- handshake_success
- handshake_after_read
- read_after_handshake
- echo_over_tls
- concurrent_tls_handshakes
- handshake_timeout

#### Estimated Effort
- 3-4 days (requires OpenSSL BIO setup)

---

### Phase 8c: `std::expected<T, E>` Migration

#### Problem

Current Socket/DB functions use `void` returns or exceptions:

```cpp
// Current
void Socket::connect(const std::string& host, uint16_t port);
// throws SocketException on failure

bool Socket::bind(const std::string& address, uint16_t port);
// returns false on failure
```

This forces callers to either catch exceptions (slow) or check bools (lose error info).

#### Solution

Replace with `std::expected<T, E>` for type-safe error handling:

```cpp
// New (C++23)
std::expected<void, SocketError> Socket::connect(
    const std::string& host, uint16_t port);

enum class SocketError {
    InvalidAddress,
    ConnectionRefused,
    Timeout,
    NetworkUnreachable,
    PermissionDenied,
    AddressInUse,
    OutOfMemory,
};
```

#### Migration Strategy

1. Add new `std::expected` overloads alongside existing APIs
2. Mark old `void`/`bool` versions as `[[deprecated]]`
3. Update internal code to use new APIs
4. Document migration path in release notes

#### Affected APIs

| Module | Functions |
|--------|-----------|
| `Socket` | `connect`, `bind`, `accept`, `recv`, `send` |
| `TLSSocket` | `performHandshake`, `read`, `write` |
| `Database` | `open`, `execute`, `query`, `queryOne` |
| `Database::Statement` | `bind`, `step`, `reset` |

#### Files

```
src/utils/Expected.h           # Common Result<T, E> alias
src/net/Socket.h               # Add expected overloads
src/net/Socket.cpp
src/net/TLSSocket.h
src/net/TLSSocket.cpp
src/db/Database.h
src/db/Database.cpp
tests/test_expected.cpp        # Tests for new APIs
```

#### Tests (~10 test cases)

- expected_success_path
- expected_error_path
- expected_chained_operations
- expected_with_void_value
- socket_connect_error
- socket_bind_error
- database_open_error
- database_query_error
- back_compat_bool_api
- back_compat_void_api

#### Estimated Effort
- 5-7 days (large refactor)

---

### Phase 8d: AsyncFile (Optional)

#### Problem

`StaticHandler::serveFile()` reads files synchronously, blocking the thread.

For high-throughput static file serving, async I/O is needed.

#### Solution

AsyncFile using thread pool (or io_uring on Linux 5.6+):

```cpp
namespace OmniNet::IO {

class AsyncFile {
public:
    explicit AsyncFile(Async::Executor& exec);
    
    // Open file (async)
    Async::Task<> open(std::string_view path, bool read = true);
    
    // Read entire file
    Async::Task<std::vector<uint8_t>> read_all();
    
    // Read range
    Async::Task<std::vector<uint8_t>> read_range(size_t offset, size_t length);
    
    // Close
    Async::Task<> close();
    
    bool isOpen() const;
    
private:
    Async::Executor& m_exec;
    int m_fd = -1;
};

}
```

#### Implementation

- **Thread pool approach** (cross-platform): Dispatch blocking `read()` to executor
- **io_uring approach** (Linux 5.6+): True async I/O via io_uring syscalls

#### Files

```
src/io/AsyncFile.h
src/io/AsyncFile.cpp
tests/test_async_file.cpp
```

#### Tests (~6 test cases)

- file_open_read
- file_read_all
- file_read_range
- file_concurrent_reads
- file_not_found
- file_large_read

#### Estimated Effort
- 3-4 days

---

### Phased Rollout

| Phase | Duration | Deliverable | Status |
|-------|----------|-------------|--------|
| **8a. AsyncLogger** | 2-3 days | Non-blocking logging | ✅ Done |
| **8b. TLS Async** | 3-4 days | Secure AsyncServer | ✅ Done |
| **8c. expected<T, E>** | 5-7 days | Modern error handling | ✅ Done |
| **8d. AsyncFile** | 3-4 days | Async static file serving | ✅ Done |
| **Release 0.4.1** | 1 day | Tag, docs, release notes | ✅ Done |

**Total**: ~2-3 weeks (can be parallelized)

---

### Migration Compatibility

All changes are **additive** and **backward-compatible**:

| Existing Code | v0.4.1 |
|---------------|--------|
| `Logger::log(level, msg)` | ✅ Still works (uses AsyncLogger internally) |
| `socket.connect(host, port)` (void) | ✅ Still works, marked `[[deprecated]]` |
| `socket.connect(host, port)` (bool) | ✅ New, returns `std::expected` |
| `TLSSocket::performHandshake()` | ✅ Still works |
| `co_await asyncTls.handshake()` | ✅ New |

---

### Performance Targets

| Metric | v0.4.0 | v0.4.1 |
|--------|--------|--------|
| Log throughput | ~10k msg/s | 1M+ msg/s (off-thread) |
| HTTPS handshake latency | 10-100ms (blocking) | Same, but non-blocking |
| Error handling overhead | Exceptions (~µs) | `expected` (~ns) |
| Static file throughput | Single-threaded | Multi-threaded |

---

### Open Questions

- [ ] **io_uring vs thread pool** for AsyncFile — depends on deployment target
- [ ] **Logger queue size** — how many messages to buffer?
- [ ] **TLS BIO mode** — use OpenSSL memory BIO or socket BIO?
- [ ] **expected<T, E> adoption strategy** — all at once or per-module?

---


## v0.7.0 Plan: Hive-Agent Feature Requests

**Status**: ✅ COMPLETE (all 6 features + the WebSocket handoff fix from prompt.txt's 2026-06-14 update)
**Target Version**: 0.7.0
**Estimated Effort**: 1-2 weeks
**Discovered**: 2026-06-14 from hive-agent (consumer of OmniNet)
**Source**: `prompt.txt` (6 feature requests, prioritized)

### Executive Summary

The hive-agent project uses OmniNet to run a long-lived daemon that
serves JSON-RPC 2.0 over HTTP POST + WebSocket. Six features were
requested to simplify their integration code. The hive-agent reports
that bugs #1–12 are all closed; this plan covers the *new* features.

| # | Feature | Severity | Consumer workaround |
|---|---------|----------|---------------------|
| 1 | `boundPort()` for ephemeral port binding | **Medium** | Requires explicit `--port` |
| 2 | WebSocket broadcast / fan-out | **High** | Tracks connections in their own `unordered_map` |
| 3 | `connectionCount()` / `clients()` for WS | **High** | Same as #2 |
| 4 | `onReady(uint16_t)` lifecycle callback | Low | Polls `isRunning()` + sleep |
| 5 | Header forwarding helpers | Low | Parses `req.header()` inline |
| 6 | `Router` typed body parsing | Low | Wraps `Json::parse` themselves |

**Priority from hive-agent**: #1 alone, or #1+#2 together (which subsumes #3).

### Scope

- **In scope** (#1, #2, #3, #4): new public API + tests.
- **In scope** (#5, #6): small convenience additions to existing classes.
- **Bonus (not in original prompt.txt but needed for #2/#3 to actually
  work end-to-end)**: full WebSocket upgrade integration into `HttpServer`
  via a `PostUpgradeCallback` mechanism. The 2026-06-14 update in
  prompt.txt noted that #2/#3's API existed but the HTTP→WS upgrade
  handoff was broken. This was fixed in commit `74d9418` — see
  "Bonus: WebSocket upgrade handoff fix" section below.

### Phase 11a: `Server::boundPort()` (smallest, unblocks most)

**Files**: `src/http/HttpServer.h`, `src/http/HttpServer.cpp`,
`tests/test_sync_http_server.cpp`.

**API**:
```cpp
class Server {
public:
    // NEW: actual bound port (via getsockname); 0 before start().
    uint16_t boundPort() const;
};
```

**Implementation**:
- `m_serverSocket` is a `std::unique_ptr<ServerSocket>`.
- `Socket::localPort()` already does `getsockname() + ntohs()`.
- Just delegate: `return m_serverSocket ? m_serverSocket->localPort() : 0;`
- ~5 lines, no new state.

**Tests** (add to `test_sync_http_server.cpp`):
| Test | Description |
|------|-------------|
| `bound_port_zero_before_start` | Returns 0 before `start()` |
| `bound_port_matches_configured` | When `port=18099`, returns 18099 |
| `bound_port_ephemeral` | When `port=0`, returns the OS-assigned port (≠0) |
| `bound_port_listenable` | `boundPort()` is the same port a client can connect to |

**Effort**: 1 hour.

### Phase 11b: WebSocket `broadcast()` / fan-out

**Files**: `src/websocket/WebSocket.h`, `src/websocket/WebSocket.cpp`,
`tests/test_websocket_broadcast.cpp` (new).

**API**:
```cpp
namespace OmniNet::WebSocket {

class Server {
public:
    using ClientId = uint64_t;

    // Register/unregister a connection (called by the app after handshake).
    ClientId addClient(Connection& conn, std::string_view path);
    void removeClient(ClientId id);

    // Send to one, or to all on a path.
    void sendTo(ClientId id, std::string_view text);
    void broadcast(std::string_view text);
    void broadcastText(std::string_view text);  // alias for clarity
    void broadcast(std::string_view text, std::string_view path);

    // Active connection set.
    std::vector<ClientId> clients() const;
    std::vector<ClientId> clients(std::string_view path) const;
    size_t connectionCount() const;          // feature #3
    size_t connectionCount(std::string_view path) const;

private:
    std::mutex m_clientsMutex;
    std::unordered_map<ClientId,
        std::pair<std::string, Connection*>> m_clients;  // path -> conn
    std::atomic<ClientId> m_nextClientId{1};
};

}
```

**Behavior**:
- `broadcast()` catches per-client send errors (one dead client doesn't
  break the others). On error, the client is auto-removed.
- `removeClient()` is also called automatically by `Connection`'s
  close handler if `setOnClose(autoRemove)` was set.
- `clients()` returns a snapshot copy (caller can iterate without holding
  the mutex).

**Tests** (`tests/test_websocket_broadcast.cpp`, ~7 tests):
| Test | Description |
|------|-------------|
| `add_remove_client` | `addClient` returns unique IDs, `removeClient` works |
| `connection_count` | `connectionCount()` reflects map size |
| `broadcast_basic` | Send to 3 mock clients, all 3 receive |
| `broadcast_by_path` | 2 clients on `/a`, 1 on `/b`; broadcast to `/a` reaches 2 |
| `broadcast_one_dead` | One client's `sendText` throws; others still receive |
| `clients_snapshot` | `clients()` returns copy, safe to iterate |
| `send_to_unknown_id` | `sendTo(invalid_id)` is a no-op, no throw |

**Effort**: 4-6 hours.

### Phase 11c: `Server::onReady(uint16_t boundPort)` callback

**Files**: `src/http/HttpServer.h`, `src/http/HttpServer.cpp`,
`tests/test_sync_http_server.cpp`.

**API**:
```cpp
class Server {
public:
    using ReadyCallback = std::function<void(uint16_t boundPort)>;
    void setOnReady(ReadyCallback cb);
    // ...
private:
    ReadyCallback m_onReady;
};
```

**Implementation**:
- In `start()`, after `m_serverSocket->bindAndListen(...)` and after
  `m_running = true`, call `m_onReady` (if set) with the bound port.
- Wrap the call in try/catch (the callback is user code, can't let
  it crash the accept loop).
- `startAsync()` needs the same: spawn the thread, the callback fires
  inside the thread, so it happens *before* `startAsync()` returns
  (the user can rely on it being called by the time `startAsync()`
  returns + scheduling delay).

**Tests**:
| Test | Description |
|------|-------------|
| `on_ready_fires` | Callback invoked exactly once after start |
| `on_ready_receives_bound_port` | Callback arg matches `boundPort()` |
| `on_ready_not_set` | No callback = no crash |
| `on_ready_throws` | Callback that throws is caught, server still runs |
| `on_ready_async_fires` | `startAsync()` eventually fires the callback |

**Effort**: 2-3 hours.

### Phase 11d: `Request` header helpers (auth + tracing)

**Files**: `src/http/HttpRequest.h`, `src/http/HttpRequest.cpp`,
`tests/test_http_request.cpp`.

**API**:
```cpp
class Request {
public:
    // Returns the value of the Authorization header, or empty view.
    std::string_view authHeader() const;

    // Returns the bearer token (strips "Bearer " prefix), or empty.
    std::string_view bearerToken() const;

    // Returns X-Trace-Id, or empty.
    std::string_view traceId() const;

    // Returns X-Request-Id, or empty.
    std::string_view requestId() const;
};
```

**Implementation**: thin wrappers around `header("Authorization")` etc.
~20 lines.

**Tests** (4 tests, simple):
| Test | Description |
|------|-------------|
| `auth_header_present` | Returns "Bearer xyz" |
| `bearer_token_strips_prefix` | "Bearer xyz" → "xyz" |
| `bearer_token_non_bearer` | "Basic xyz" → "" (only Bearer scheme) |
| `trace_id_present` | Returns X-Trace-Id value |

**Effort**: 1-2 hours.

### Phase 11e: Router typed body parsing (JSON handlers)

**Files**: `src/http/Router.h`, `src/http/Router.cpp`,
`tests/test_router_json.cpp` (new).

**API**:
```cpp
class Router {
public:
    using JsonHandler = std::function<void(const nlohmann::json&, Response&)>;

    // Typed overloads — body auto-parsed; 400 on bad JSON.
    void get(std::string pattern, JsonHandler handler);    // for completeness
    void post(std::string pattern, JsonHandler handler);
    void put(std::string pattern, JsonHandler handler);
    void patch(std::string pattern, JsonHandler handler);
    // ... etc
};
```

**Implementation**:
- Wrap the user's `JsonHandler` into a `Handler` that:
  1. Reads `req.body()` (already a `std::string_view`)
  2. Calls `nlohmann::json::parse(body)` in try/catch
  3. On success: invokes the user handler with the parsed JSON
  4. On failure: `resp = Response::badRequest("Invalid JSON: " + e.what())`
- Header: `#include <nlohmann/json.hpp>`
- Requires the application to call `omni::init()` (already initializes
  nlohmann/json via FetchContent, no new deps).

**Tests** (~6 tests):
| Test | Description |
|------|-------------|
| `json_handler_valid` | `{"a":1}` → handler called with parsed JSON |
| `json_handler_invalid` | `{not json}` → 400, handler not called |
| `json_handler_empty_body` | `""` → 400 |
| `json_handler_no_content_type` | Body parsed even without `Content-Type: application/json` |
| `json_handler_complex` | Nested objects/arrays round-trip |
| `json_post_and_get` | Both `post(pat, JsonHandler)` and `get(pat, JsonHandler)` work |

**Effort**: 3-4 hours.

### Phased Rollout

| Phase | Feature | Effort | Tests | Status |
|-------|---------|--------|-------|--------|
| **11a** | `Server::boundPort()` | 1h | +4 | ✅ `c265603` |
| **11b** | WS broadcast + clients | 5h | +10 | ✅ `c265603` |
| **11c** | `Server::onReady()` | 3h | +5 | ✅ `c265603` |
| **11d** | Header helpers | 2h | 0 dedicated | ✅ `c265603` |
| **11e** | Router JSON handlers | 4h | 0 dedicated | ✅ `c265603` |
| **11f** | **Bonus**: WS upgrade handoff fix | — | +2 | ✅ `74d9418` |
| **Tag & release** | 0.7.0-rc1 | 1h | — | ⏳ |

**Total**: ~15-20 hours of focused work; 1-2 calendar days.
**Actual**: 4 commits (`c265603`, `7dea30f`, `c017d09`, `74d9418`).

### Test Results (all verified)

- `test_websocket_broadcast`: **10/10 PASS** (existing API tests)
- `test_websocket_echo` (new, commit `74d9418`): **2/2 PASS**
  - `handshake_returns_101_no_content_length` — Bug #1
  - `echo_round_trip` — Bug #2 + #3 + end-to-end echo
- `test_sync_http_server`: **18/18 PASS** (including 9 new tests
  for `boundPort` and `onReady`)
- `test_http_response`, `test_token`, `test_config`, etc.: all pass
- Full build: clean, all 40+ targets compile

### Bonus: WebSocket upgrade handoff fix (commit `74d9418`)

The 2026-06-14 update in `prompt.txt` noted that features #2/#3
(broadcast / connection tracking) had the right *API* but the
HTTP→WS upgrade handoff was broken — the `Connection&` passed to
the per-path handler was constructed with a null socket, and
`addClient()` was never called. This was fixed by:

1. **`Response::markProtocolSwitch()` / `isProtocolSwitch()`** — flag
   for the 101 path.
2. **`HttpServer::setPostUpgradeHandler(path, cb)`** — registers a
   callback that receives the raw socket after the 101 is written.
3. **`Connection::setPostUpgradeCallback(cb)`** + protocol-switch
   handoff in `handleNextRequest()` — the Connection releases the
   socket to the callback after writing.
4. **`WebSocket::Server::on()` rewritten** — now uses the
   post-upgrade callback to create the `WebSocket::Connection`,
   call `addClient()`, and spawn the handler thread.
5. **`Connection::readLoop()`** — blocking frame-read loop used by
   handler threads to dispatch frames to `onText`/`onBinary`/`onClose`.
6. **`Response::build()` suppresses Content-Length for 101/204/304**
   (RFC 7230 §3.3.2, RFC 6455 §1.3).
7. **Side fix: `Connection` server-side ctor sets `m_open = true`** —
   was default-initialized to `false`, which made `addClient()`
   reject the connection with `id=0`.
8. **Side fix: `Connection::close()` checks for null socket** —
   prevents segfault in `MockConnection` (test mock with null
   socket) when the destructor calls `close()`.

**Regression test**: `tests/test_websocket_echo.cpp` — raw-socket
client does a full WebSocket handshake, sends a masked text frame,
and verifies the echo. This test would have caught all 3 bugs.

**End-to-end flow now works**:

```
Client                     HttpServer                WebSocket::Server
  │                           │                           │
  ├── GET /ws (Upgrade) ────►│                           │
  │                           ├─ route handler sets 101   │
  │                           │  + markProtocolSwitch()  │
  │                           ├─ writes 101 response ────►│
  │◄── 101 Switching ─────────┤                           │
  │                           ├─ releases socket ────────►│
  │                           │                           ├─ create Connection
  │                           │                           ├─ addClient(conn,"/ws")
  │                           │                           ├─ spawn handler thread
  │                           │                           │   └─ readLoop() blocks
  ├── text frame ──────────────────────────────────────────►
  │                                                            ├─ onText callback
  │◄── "echo: hello" ───────────────────────────────────────┤
```

### Files Touched

```
src/http/HttpServer.h            (11a, 11c)
src/http/HttpServer.cpp          (11a, 11c)
src/http/HttpRequest.h           (11d)
src/http/HttpRequest.cpp         (11d)
src/http/Router.h                (11e)
src/http/Router.cpp              (11e)
src/websocket/WebSocket.h        (11b)
src/websocket/WebSocket.cpp      (11b)
tests/test_sync_http_server.cpp  (11a, 11c — extend existing)
tests/test_http_request.cpp      (11d — extend existing)
tests/test_websocket_broadcast.cpp (11b — new)
tests/test_router_json.cpp       (11e — new)
tests/CMakeLists.txt             (add 2 new test targets)
```

### Test Plan Summary

- 4 tests for #1 (`boundPort`)
- 7 tests for #2/#3 (broadcast + clients)
- 5 tests for #4 (`onReady`)
- 4 tests for #5 (header helpers)
- 6 tests for #6 (JSON handlers)
- **= 26 new tests**, plus the existing 152 (post-bugfix-12) = **178 total**

### Backward Compatibility

All changes are **purely additive**:
- New methods on existing classes (no signature changes)
- `Router::post(...)` already exists with a `Handler` signature;
  the new `post(pat, JsonHandler)` is an overload (C++ resolves by
  the lambda's signature; `Handler` is `std::function<void(const Request&, Response&)>`
  and `JsonHandler` is `std::function<void(const json&, Response&)>` —
  unambiguous).
- No headers moved, no namespaces renamed.

### Risk & Open Questions

- [ ] **`addClient` registration timing** (Phase 11b): the consumer
      has to call this after the handshake. The current WS `Server::on()`
      placeholder doesn't expose a callback for "handshake complete".
      The consumer (hive-agent) is fine with a manual `addClient` call
      from inside their per-connection handler — that's the only sane
      option given the half-finished upgrade code. Document this clearly.
- [ ] **`startAsync()` + `onReady`** (Phase 11c): there's a race where
      `startAsync()` returns *before* the callback fires (the thread
      has to be scheduled and run the bind). Document that the user
      must wait for the callback or poll `boundPort()` to be non-zero.
- [ ] **JSON handler for non-POST methods** (Phase 11e): GET shouldn't
      usually have a body. The overload exists for completeness but
      the wrapper just returns 400 on empty body. Document this.
