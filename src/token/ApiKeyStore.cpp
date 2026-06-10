// OmniNet - API Key Store Implementation
#include "ApiKeyStore.h"
#include "TokenUtils.h"
#include "utils/Utils.h"
#include "utils/Exception.h"

#include <chrono>

namespace OmniNet::Token {

namespace {

constexpr const char* CREATE_SCHEMA = R"(
    CREATE TABLE IF NOT EXISTS api_keys (
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
    )
)";

constexpr const char* CREATE_INDEX_USER = R"(
    CREATE INDEX IF NOT EXISTS idx_keys_user ON api_keys(user_id)
)";

constexpr const char* CREATE_INDEX_HASH = R"(
    CREATE INDEX IF NOT EXISTS idx_keys_hash ON api_keys(key_hash)
)";

constexpr const char* CREATE_INDEX_PREFIX = R"(
    CREATE INDEX IF NOT EXISTS idx_keys_prefix ON api_keys(key_prefix)
)";

} // anonymous namespace

// ============================================================================
// API Key Store
// ============================================================================

ApiKeyStore::ApiKeyStore(DB::Database& db)
    : m_db(db) {
}

void ApiKeyStore::initialize() {
    m_db.execute(CREATE_SCHEMA);
    m_db.execute(CREATE_INDEX_USER);
    m_db.execute(CREATE_INDEX_HASH);
    m_db.execute(CREATE_INDEX_PREFIX);
}

ApiKeyStore::CreateResult ApiKeyStore::create(
    std::string_view userId,
    std::string_view name,
    std::vector<std::string> scopes,
    std::chrono::seconds ttl
) {
    return create(userId, name, scopes, ttl, {}, {}, std::nullopt, {});
}

ApiKeyStore::CreateResult ApiKeyStore::create(
    std::string_view userId,
    std::string_view name,
    std::vector<std::string> scopes,
    std::chrono::seconds ttl,
    std::string_view ip,
    std::string_view userAgent,
    std::optional<uint64_t> useLimit,
    std::string_view metadata
) {
    // Generate prefix (default prefix + random suffix)
    std::string prefix = generateUniquePrefix();
    
    // Generate full API key with the prefix
    std::string rawKey = generateApiKey(prefix);
    
    // Hash the full key for storage
    std::string keyHash = hashToken(rawKey);
    
    // Generate public ID (same as prefix for API keys)
    std::string id = prefix;
    
    // Calculate timestamps
    auto now = std::chrono::system_clock::now();
    std::optional<std::chrono::system_clock::time_point> expires;
    if (ttl > std::chrono::seconds::zero()) {
        expires = now + ttl;
    }
    
    // Format timestamps
    std::string createdStr = Utils::formatIso8601(now);
    std::string expiresStr = expires ? Utils::formatIso8601(*expires) : "";
    
    // Serialize scopes to JSON
    std::string scopesJson = "[";
    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i > 0) scopesJson += ",";
        scopesJson += "\"" + scopes[i] + "\"";
    }
    scopesJson += "]";
    
    // Handle empty metadata
    std::string metadataStr(metadata);
    if (metadataStr.empty()) {
        metadataStr = "{}";
    }
    
    // Insert into database
    auto stmt = m_db.prepare(
        "INSERT INTO api_keys "
        "(id, key_hash, key_prefix, user_id, name, scopes, created_at, expires_at, "
        "last_ip, metadata, use_limit) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    
    stmt.bind(1, id);
    stmt.bind(2, keyHash);
    stmt.bind(3, prefix);
    stmt.bind(4, std::string(userId));
    stmt.bind(5, std::string(name));
    stmt.bind(6, scopesJson);
    stmt.bind(7, createdStr);
    if (expires.has_value()) {
        stmt.bind(8, expiresStr);
    } else {
        stmt.bind(8, nullptr);
    }
    // C++23: ternary with nullptr/string not allowed; use explicit if
    if (ip.empty()) {
        stmt.bind(9, nullptr);
    } else {
        stmt.bind(9, std::string(ip));
    }
    stmt.bind(10, metadataStr);
    if (useLimit) {
        stmt.bind(11, static_cast<int64_t>(*useLimit));
    } else {
        stmt.bind(11, nullptr);
    }
    
    stmt.step();
    stmt.reset();
    
    return {rawKey, id};
}

ApiKeyStore::ValidationResult ApiKeyStore::validate(std::string_view rawKey) {
    // Validate format first
    if (!isValidApiKeyFormat(rawKey)) {
        return ValidationResult::failure("invalid_format");
    }
    
    // Hash the provided key
    std::string keyHash = hashToken(rawKey);
    
    // Lookup by hash
    auto stmt = m_db.prepare(
        "SELECT id, user_id, name, scopes, created_at, expires_at, last_used_at, "
        "last_ip, revoked, revoked_at, use_count, use_limit, metadata "
        "FROM api_keys "
        "WHERE key_hash = ?"
    );
    
    stmt.bind(1, keyHash);
    
    if (!stmt.step()) {
        return ValidationResult::failure("not_found");
    }
    
    auto key = parseKeyFromStatement(stmt);
    
    // Check if revoked
    if (key.revoked) {
        return ValidationResult::failure("revoked");
    }
    
    // Check if expired
    if (key.isExpired()) {
        return ValidationResult::failure("expired");
    }
    
    // Check use limit
    if (key.useLimit && key.useCount >= *key.useLimit) {
        return ValidationResult::failure("usage_limit");
    }
    
    return ValidationResult::success(std::move(key));
}

ApiKeyStore::ValidationResult ApiKeyStore::validateAndUse(
    std::string_view rawKey,
    std::string_view currentIp
) {
    // First validate
    auto result = validate(rawKey);
    if (!result.valid) {
        return result;
    }
    
    // Hash the key
    std::string keyHash = hashToken(rawKey);
    
    // Update usage statistics
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto updateStmt = m_db.prepare(
        "UPDATE api_keys "
        "SET use_count = use_count + 1, last_used_at = ?, last_ip = ? "
        "WHERE key_hash = ?"
    );
    
    updateStmt.bind(1, nowStr);
    updateStmt.bind(2, std::string(currentIp));
    updateStmt.bind(3, keyHash);
    
    updateStmt.step();
    updateStmt.reset();
    
    // Re-fetch the updated key
    auto selectStmt = m_db.prepare(
        "SELECT id, user_id, name, scopes, created_at, expires_at, last_used_at, "
        "last_ip, revoked, revoked_at, use_count, use_limit, metadata "
        "FROM api_keys "
        "WHERE key_hash = ?"
    );
    
    selectStmt.bind(1, keyHash);
    
    if (!selectStmt.step()) {
        return ValidationResult::failure("not_found");
    }
    
    result.key = parseKeyFromStatement(selectStmt);
    return result;
}

bool ApiKeyStore::hasScope(const ApiKey& key, std::string_view requiredScope) const {
    return key.hasScope(requiredScope);
}

bool ApiKeyStore::revoke(std::string_view rawKey) {
    std::string keyHash = hashToken(rawKey);
    
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE api_keys "
        "SET revoked = 1, revoked_at = ? "
        "WHERE key_hash = ?"
    );
    
    stmt.bind(1, nowStr);
    stmt.bind(2, keyHash);
    
    stmt.step();
    stmt.reset();
    
    auto selectStmt = m_db.prepare("SELECT changes()");
    if (selectStmt.step()) {
        return selectStmt.columnInt64(0) > 0;
    }
    
    return false;
}

bool ApiKeyStore::revokeById(std::string_view keyId) {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE api_keys "
        "SET revoked = 1, revoked_at = ? "
        "WHERE id = ?"
    );
    
    stmt.bind(1, nowStr);
    stmt.bind(2, std::string(keyId));
    
    stmt.step();
    stmt.reset();
    
    auto selectStmt = m_db.prepare("SELECT changes()");
    if (selectStmt.step()) {
        return selectStmt.columnInt64(0) > 0;
    }
    
    return false;
}

size_t ApiKeyStore::revokeAllForUser(std::string_view userId) {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE api_keys "
        "SET revoked = 1, revoked_at = ? "
        "WHERE user_id = ? AND revoked = 0"
    );
    
    stmt.bind(1, nowStr);
    stmt.bind(2, std::string(userId));
    
    stmt.step();
    stmt.reset();
    
    auto selectStmt = m_db.prepare("SELECT changes()");
    if (selectStmt.step()) {
        return static_cast<size_t>(selectStmt.columnInt64(0));
    }
    
    return 0;
}

std::optional<ApiKey> ApiKeyStore::getById(std::string_view keyId) {
    auto stmt = m_db.prepare(
        "SELECT id, user_id, name, scopes, created_at, expires_at, last_used_at, "
        "last_ip, revoked, revoked_at, use_count, use_limit, metadata "
        "FROM api_keys "
        "WHERE id = ?"
    );
    
    stmt.bind(1, std::string(keyId));
    
    if (!stmt.step()) {
        return std::nullopt;
    }
    
    return parseKeyFromStatement(stmt);
}

std::vector<ApiKey> ApiKeyStore::listForUser(
    std::string_view userId,
    bool activeOnly
) {
    std::vector<ApiKey> keys;
    
    auto now = Utils::formatIso8601(std::chrono::system_clock::now());
    
    if (activeOnly) {
        auto stmt = m_db.prepare(
            "SELECT id, user_id, name, scopes, created_at, expires_at, last_used_at, "
            "last_ip, revoked, revoked_at, use_count, use_limit, metadata "
            "FROM api_keys "
            "WHERE user_id = ? AND revoked = 0 AND "
            "(expires_at IS NULL OR expires_at > ?) "
            "ORDER BY created_at DESC"
        );
        stmt.bind(1, std::string(userId));
        stmt.bind(2, now);
        
        while (stmt.step()) {
            keys.push_back(parseKeyFromStatement(stmt));
        }
    } else {
        auto stmt = m_db.prepare(
            "SELECT id, user_id, name, scopes, created_at, expires_at, last_used_at, "
            "last_ip, revoked, revoked_at, use_count, use_limit, metadata "
            "FROM api_keys "
            "WHERE user_id = ? "
            "ORDER BY created_at DESC"
        );
        stmt.bind(1, std::string(userId));
        
        while (stmt.step()) {
            keys.push_back(parseKeyFromStatement(stmt));
        }
    }
    
    return keys;
}

size_t ApiKeyStore::cleanup() {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    // Count expired before delete
    auto countStmt = m_db.prepare(
        "SELECT COUNT(*) FROM api_keys WHERE expires_at IS NOT NULL AND expires_at < ?"
    );
    countStmt.bind(1, nowStr);
    
    size_t beforeCount = 0;
    if (countStmt.step()) {
        beforeCount = static_cast<size_t>(countStmt.columnInt64(0));
    }
    
    // Delete expired keys
    auto deleteStmt = m_db.prepare(
        "DELETE FROM api_keys WHERE expires_at IS NOT NULL AND expires_at < ?"
    );
    deleteStmt.bind(1, nowStr);
    deleteStmt.step();
    deleteStmt.reset();
    
    // Also delete revoked keys older than 30 days
    auto oldRevoked = now - std::chrono::days(30);
    std::string oldRevokedStr = Utils::formatIso8601(oldRevoked);
    
    auto deleteRevokedStmt = m_db.prepare(
        "DELETE FROM api_keys WHERE revoked = 1 AND revoked_at < ?"
    );
    deleteRevokedStmt.bind(1, oldRevokedStr);
    deleteRevokedStmt.step();
    deleteRevokedStmt.reset();
    
    return beforeCount;
}

size_t ApiKeyStore::count() const {
    auto result = m_db.queryOne("SELECT COUNT(*) FROM api_keys");
    if (result && !result->empty()) {
        return static_cast<size_t>(std::stoll((*result)[0]));
    }
    return 0;
}

size_t ApiKeyStore::countActive() const {
    auto now = Utils::formatIso8601(std::chrono::system_clock::now());
    
    auto stmt = m_db.prepare(
        "SELECT COUNT(*) FROM api_keys "
        "WHERE revoked = 0 AND (expires_at IS NULL OR expires_at > ?)"
    );
    stmt.bind(1, now);
    
    size_t count = 0;
    if (stmt.step()) {
        count = static_cast<size_t>(stmt.columnInt64(0));
    }
    
    return count;
}

size_t ApiKeyStore::countForUser(std::string_view userId) const {
    auto stmt = m_db.prepare("SELECT COUNT(*) FROM api_keys WHERE user_id = ?");
    stmt.bind(1, std::string(userId));
    
    size_t count = 0;
    if (stmt.step()) {
        count = static_cast<size_t>(stmt.columnInt64(0));
    }
    
    return count;
}

void ApiKeyStore::setDefaultPrefix(std::string_view prefix) {
    m_defaultPrefix = std::string(prefix);
}

std::string ApiKeyStore::generateUniquePrefix() {
    // Format: <default_prefix>_<random_suffix>
    // e.g., "omn_sk_a3f8b2c1"
    std::string suffix = generateShortId(8);
    return m_defaultPrefix + "_" + suffix;
}

// ============================================================================
// Private Helpers
// ============================================================================

ApiKey ApiKeyStore::parseKeyFromStatement(const DB::Database::Statement& stmt) {
    ApiKey key;
    
    key.id = stmt.columnText(0);
    key.userId = stmt.columnText(1);
    key.name = stmt.columnText(2);
    
    // Parse scopes from JSON
    std::string scopesJson = stmt.columnText(3);
    key.scopes = parseScopesFromJson(scopesJson);
    
    // Parse timestamps
    auto createdAt = Utils::parseIso8601(stmt.columnText(4));
    if (createdAt) key.createdAt = *createdAt;
    
    if (!stmt.columnIsNull(5)) {
        auto expiresAt = Utils::parseIso8601(stmt.columnText(5));
        if (expiresAt) key.expiresAt = expiresAt;
    }
    
    if (!stmt.columnIsNull(6)) {
        auto lastUsedAt = Utils::parseIso8601(stmt.columnText(6));
        if (lastUsedAt) key.lastUsedAt = lastUsedAt;
    }
    
    if (!stmt.columnIsNull(7)) {
        key.lastIp = stmt.columnText(7);
    }
    
    key.revoked = stmt.columnInt64(8) != 0;
    
    if (!stmt.columnIsNull(9)) {
        auto revokedAt = Utils::parseIso8601(stmt.columnText(9));
        if (revokedAt) key.revokedAt = revokedAt;
    }
    
    key.useCount = static_cast<uint64_t>(stmt.columnInt64(10));
    
    if (!stmt.columnIsNull(11)) {
        key.useLimit = static_cast<uint64_t>(stmt.columnInt64(11));
    }
    
    key.metadata = stmt.columnText(12);
    
    return key;
}

std::vector<std::string> ApiKeyStore::parseScopesFromJson(const std::string& json) {
    std::vector<std::string> scopes;
    
    // Simple JSON array parser (handles ["scope1", "scope2"])
    if (json.empty() || json == "[]") return scopes;
    
    size_t pos = 0;
    while (pos < json.size()) {
        // Find opening quote
        size_t start = json.find('"', pos);
        if (start == std::string::npos) break;
        
        // Find closing quote
        size_t end = json.find('"', start + 1);
        if (end == std::string::npos) break;
        
        // Extract scope
        scopes.push_back(json.substr(start + 1, end - start - 1));
        
        // Move past closing quote and comma
        pos = end + 1;
        
        // Skip comma and whitespace
        while (pos < json.size() && (json[pos] == ',' || json[pos] == ' ')) {
            ++pos;
        }
    }
    
    return scopes;
}

} // namespace OmniNet::Token