# Token System API

## Overview

OmniNet provides secure token management for session authentication and API key authorization. The token system uses industry-standard cryptographic practices:

- **256-bit entropy** - Session tokens use 32 bytes of cryptographically secure random data
- **SHA-256 hashing** - Raw tokens are never stored, only their hashes
- **Constant-time comparison** - Prevents timing attacks during validation
- **SQL injection protection** - All database operations use prepared statements

## Token Utils

Core utilities for token generation and validation.

### Header
```cpp
#include "token/TokenUtils.h"
using namespace OmniNet::Token;
```

### Generation Functions

```cpp
// Generate hex-encoded token (32 bytes = 64 hex chars by default)
std::string token = generateToken();        // 64 chars
std::string token = generateToken(16);     // 32 chars

// Generate random bytes (binary)
std::vector<uint8_t> bytes = generateBytes(32);

// Generate API key with prefix (e.g., "myapp_a3f8b2c1d4e5...")
std::string apiKey = generateApiKey("myapp");

// Generate short ID (e.g., "a3f8b2c1")
std::string shortId = generateShortId(4);  // 8 hex chars
```

### Hashing Functions

```cpp
// Hash token (SHA-256, returns hex string)
std::string hash = hashToken(rawToken);

// Hash to bytes
std::vector<uint8_t> hashBytes = hashTokenBytes(rawToken);
```

### Validation Functions

```cpp
// Check if hex token has valid format
bool valid = isValidHexFormat(token, 32);  // true if 64 hex chars

// Check if API key has valid format (prefix_secret)
bool valid = isValidApiKeyFormat(apiKey, 3);  // prefix >= 3 chars

// Check if string is valid hex
bool valid = isHexCharset(hexString);
```

### Extraction Functions

```cpp
// Extract prefix (before first underscore)
std::string prefix = extractPrefix("myapp_a3f8...");  // "myapp"

// Extract secret (after first underscore)
std::string secret = extractSecret("myapp_a3f8...");  // "a3f8..."
```

### Base32 Encoding

```cpp
// Encode bytes to Crockford's base32
std::string encoded = base32Encode(bytes);

// Decode base32 to bytes
auto decoded = base32Decode(encoded);  // returns std::optional<std::vector<uint8_t>>
```

### Security Functions

```cpp
// Constant-time comparison (prevents timing attacks)
bool equal = secureCompare(a, b);

// For byte vectors
bool equal = secureCompareBytes(bytesA, bytesB);
```

---

## Session Token Store

Secure session token storage with SQLite backend.

### Header
```cpp
#include "token/SessionTokenStore.h"
using namespace OmniNet::Token;
```

### SessionToken Structure

```cpp
struct SessionToken {
    std::string id;                                       // Public token ID
    std::string userId;                                  // Associated user
    std::chrono::system_clock::time_point createdAt;    // Creation time
    std::chrono::system_clock::time_point expiresAt;   // Expiration time
    std::optional<time_point> lastUsedAt;                // Last usage
    std::optional<std::string> lastIp;                  // Last IP
    std::optional<std::string> userAgent;               // Last user-agent
    bool revoked = false;                               // Revocation flag
    std::optional<time_point> revokedAt;                // Revocation time
    std::string metadata;                               // JSON metadata
    
    bool isExpired() const;
    bool isValid() const;
    std::chrono::seconds remainingLifetime() const;
};
```

### Basic Usage

```cpp
SessionTokenStore store(db);
store.initialize();

// Create session token (returns raw token - store securely!)
std::string rawToken = store.create(
    "user_123",                          // User ID
    std::chrono::hours(24),              // TTL (24 hours)
    "192.168.1.1",                      // IP address
    "Mozilla/5.0",                       // User-Agent
    R"({"purpose": "web"})"             // JSON metadata
);
// WARNING: rawToken is ONLY shown at creation!

// Validate token
auto session = store.validate(rawToken);
if (session && session->isValid()) {
    std::cout << "User: " << session->userId << "\n";
    std::cout << "Expires in: " << session->remainingLifetime().count() << "s\n";
}

// Validate and update usage
auto session = store.validateAndTouch(rawToken, "10.0.0.1");
```

### Token Revocation

```cpp
// Revoke single token
bool revoked = store.revoke(rawToken);

// Revoke by token ID
bool revoked = store.revokeById("a3f8b2c1");

// Revoke all tokens for user (logout everywhere)
size_t count = store.revokeAllForUser("user_123");
std::cout << "Revoked " << count << " tokens\n";
```

### Queries

```cpp
// Get token by ID (cannot be used to validate - raw token not stored)
auto session = store.getById("a3f8b2c1");

// List all tokens for user
auto tokens = store.listForUser("user_123");

// List only active tokens
auto activeTokens = store.listForUser("user_123", true);
```

### Cleanup

```cpp
// Remove expired tokens (run periodically)
size_t removed = store.cleanup();
std::cout << "Removed " << removed << " expired tokens\n";
```

### Statistics

```cpp
size_t total = store.count();
size_t active = store.countActive();
size_t userCount = store.countForUser("user_123");
```

---

## API Key Store

Secure API key management with scopes and expiration.

### Header
```cpp
#include "token/ApiKeyStore.h"
using namespace OmniNet::Token;
```

### ApiKey Structure

```cpp
struct ApiKey {
    std::string id;                                       // Public key ID (prefix)
    std::string userId;                                  // Owner user ID
    std::string name;                                    // Human-readable name
    std::vector<std::string> scopes;                    // Permission scopes
    std::chrono::system_clock::time_point createdAt;    // Creation time
    std::optional<time_point> expiresAt;                 // Expiration (null = never)
    std::optional<time_point> lastUsedAt;               // Last usage
    std::optional<std::string> lastIp;                  // Last IP
    bool revoked = false;                               // Revocation flag
    std::optional<time_point> revokedAt;                 // Revocation time
    uint64_t useCount = 0;                            // Usage count
    std::optional<uint64_t> useLimit;                    // Max uses (null = unlimited)
    std::string metadata;                               // JSON metadata
    
    bool isExpired() const;
    bool isValid() const;
    bool hasScope(std::string_view scope) const;
    std::optional<uint64_t> remainingUses() const;
};
```

### Basic Usage

```cpp
ApiKeyStore store(db);
store.initialize();

// Create API key
auto result = store.create(
    "user_123",                              // User ID
    "Production API Key",                    // Name
    {"read", "write:orders"},               // Scopes
    std::chrono::days(365)                  // Expires in 1 year (0 = never)
);
// WARNING: result.rawKey is ONLY shown at creation!

std::cout << "API Key: " << result.rawKey << "\n";
std::cout << "Key ID: " << result.id << "\n";

// Validate key
auto validation = store.validate(result.rawKey);
if (validation.valid) {
    // Check scopes
    if (store.hasScope(*validation.key, "read")) {
        std::cout << "Read access granted\n";
    }
}
```

### Validation Result

```cpp
struct ValidationResult {
    bool valid;
    std::optional<ApiKey> key;
    std::string error;  // "expired", "revoked", "not_found", "usage_limit"
    
    static ValidationResult success(ApiKey key);
    static ValidationResult failure(std::string error);
};

// Validate and track usage
auto result = store.validateAndUse(rawKey, "10.0.0.1");
if (result.valid) {
    std::cout << "Key used " << result.key->useCount << " times\n";
}
```

### Scopes

Scopes follow a hierarchical pattern: `resource:action`

```cpp
// Create key with various scopes
store.create(userId, "Admin Key", {"*"});
store.create(userId, "Read-Only", {"read"});
store.create(userId, "Orders Manager", {"read", "write:orders", "delete:orders"});
store.create(userId, "Users Admin", {"admin:users", "admin:roles"});

// Scope checking (supports wildcards)
if (key.hasScope("admin:users")) { /* ... */ }
if (key.hasScope("*")) { /* admin access */ }
```

### Key Revocation

```cpp
// Revoke by raw key
store.revoke(rawKey);

// Revoke by key ID (prefix)
store.revokeById("omn_sk_a3f8b2c1");

// Revoke all keys for user
size_t count = store.revokeAllForUser("user_123");
```

### Usage Limits

```cpp
// Create key with limited uses (e.g., for one-time API access)
auto result = store.create(
    "user_123",
    "One-Time Key",
    {"read"},
    std::chrono::seconds::zero(),  // No expiration
    "", "", "",
    1  // useLimit = 1
);

// Validation fails when limit reached
auto validation = store.validate(result.rawKey);
if (!validation.valid && validation.error == "usage_limit") {
    std::cout << "Key has been used up\n";
}
```

### Queries

```cpp
// Get key by ID
auto key = store.getById("omn_sk_a3f8b2c1");

// List all keys for user
auto keys = store.listForUser("user_123", true);  // activeOnly=true
```

---

## Database Schema

The token stores create these tables automatically:

### session_tokens
```sql
CREATE TABLE session_tokens (
    id              TEXT PRIMARY KEY,
    token_hash      TEXT NOT NULL UNIQUE,
    user_id         TEXT NOT NULL,
    created_at      TEXT NOT NULL,
    expires_at      TEXT NOT NULL,
    last_used_at    TEXT,
    last_ip         TEXT,
    user_agent      TEXT,
    revoked         INTEGER DEFAULT 0,
    revoked_at      TEXT,
    metadata        TEXT DEFAULT '{}'
);

CREATE INDEX idx_tokens_user ON session_tokens(user_id);
CREATE INDEX idx_tokens_expires ON session_tokens(expires_at);
CREATE INDEX idx_tokens_hash ON session_tokens(token_hash);
```

### api_keys
```sql
CREATE TABLE api_keys (
    id              TEXT PRIMARY KEY,
    key_hash        TEXT NOT NULL,
    key_prefix      TEXT NOT NULL,
    user_id         TEXT NOT NULL,
    name            TEXT NOT NULL,
    scopes          TEXT NOT NULL,
    created_at      TEXT NOT NULL,
    expires_at      TEXT,
    last_used_at    TEXT,
    last_ip         TEXT,
    revoked         INTEGER DEFAULT 0,
    revoked_at      TEXT,
    use_count       INTEGER DEFAULT 0,
    use_limit       INTEGER,
    metadata        TEXT DEFAULT '{}'
);

CREATE INDEX idx_keys_user ON api_keys(user_id);
CREATE INDEX idx_keys_hash ON api_keys(key_hash);
CREATE INDEX idx_keys_prefix ON api_keys(key_prefix);
```

---

## Security Best Practices

1. **Never log raw tokens** - Only store and compare hashes
2. **Transmit tokens securely** - Always use HTTPS/TLS
3. **Set appropriate TTLs** - Short-lived tokens are more secure
4. **Use scopes liberally** - Principle of least privilege
5. **Implement token rotation** - Regularly rotate API keys
6. **Monitor token usage** - Track validation patterns
7. **Revoke compromised tokens immediately** - Use `revokeAllForUser()` if needed

---

## Example: Full Authentication Flow

```cpp
#include "token/SessionTokenStore.h"
#include "token/ApiKeyStore.h"

using namespace OmniNet::Token;

// Initialize stores
SessionTokenStore sessionStore(db);
ApiKeyStore apiKeyStore(db);
sessionStore.initialize();
apiKeyStore.initialize();

// --- User Login: Create Session ---
std::string sessionToken = sessionStore.create(
    user.id,
    std::chrono::hours(24),
    req.clientIp(),
    req.header("User-Agent"),
    R"({"method": "password"})"
);

// Send token to client (in cookie or response body)
resp.setCookie("session", sessionToken, 
    Cookie::Options{}.maxAge(86400).httpOnly(true));

// --- API Access: Validate API Key ---
auto validation = apiKeyStore.validateAndUse(
    req.header("X-API-Key"),
    req.clientIp()
);

if (!validation.valid) {
    resp.setStatus(StatusCode::Unauthorized);
    resp.setBody(R"({"error": ")" + validation.error + R"("})");
    return;
}

// Check required scope
if (!apiKeyStore.hasScope(*validation.key, "write:orders")) {
    resp.setStatus(StatusCode::Forbidden);
    resp.setBody(R"({"error": "Insufficient permissions"})");
    return;
}

// --- User Logout: Revoke Session ---
// Get token from cookie
auto sessionToken = req.cookie("session");
if (sessionToken) {
    sessionStore.revoke(*sessionToken);
}

// Clear cookie
resp.setCookie("session", "", Cookie::Options{}.maxAge(0));
```

---

## See Also

- [Database API](database.md) - SQLite wrapper
- [HTTP Server API](http-server.md) - HTTP request/response
