// OmniNet - Session Token Store Implementation
#include "SessionTokenStore.h"
#include "TokenUtils.h"
#include "utils/Utils.h"
#include "utils/Exception.h"

#include <chrono>
#include <mutex>

namespace OmniNet::Token {

namespace {

// ============================================================================
// SQL Statements
// ============================================================================

constexpr const char* CREATE_SCHEMA = R"(
    CREATE TABLE IF NOT EXISTS session_tokens (
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
    )
)";

constexpr const char* CREATE_INDEX_USER = R"(
    CREATE INDEX IF NOT EXISTS idx_tokens_user ON session_tokens(user_id)
)";

constexpr const char* CREATE_INDEX_EXPIRES = R"(
    CREATE INDEX IF NOT EXISTS idx_tokens_expires ON session_tokens(expires_at)
)";

constexpr const char* CREATE_INDEX_HASH = R"(
    CREATE INDEX IF NOT EXISTS idx_tokens_hash ON session_tokens(token_hash)
)";

} // anonymous namespace

// ============================================================================
// Session Token Store
// ============================================================================

SessionTokenStore::SessionTokenStore(DB::Database& db)
    : m_db(db) {
}

void SessionTokenStore::initialize() {
    m_db.execute(CREATE_SCHEMA);
    m_db.execute(CREATE_INDEX_USER);
    m_db.execute(CREATE_INDEX_EXPIRES);
    m_db.execute(CREATE_INDEX_HASH);
}

std::string SessionTokenStore::create(
    std::string_view userId,
    std::chrono::seconds ttl,
    std::string_view metadata
) {
    return create(userId, ttl, {}, {}, metadata);
}

std::string SessionTokenStore::create(
    std::string_view userId,
    std::chrono::seconds ttl,
    std::string_view ip,
    std::string_view userAgent,
    std::string_view metadata
) {
    // Generate 32 bytes of random data (256 bits entropy)
    std::string rawToken = generateToken(32);
    
    // Hash the token for storage (never store raw!)
    std::string tokenHash = hashToken(rawToken);
    
    // Generate public ID (8 hex chars)
    std::string id = generateShortId(8);
    
    // Calculate timestamps
    auto now = std::chrono::system_clock::now();
    auto expires = now + ttl;
    
    // Format timestamps as ISO 8601
    std::string createdStr = Utils::formatIso8601(now);
    std::string expiresStr = Utils::formatIso8601(expires);
    
    // Handle empty metadata
    std::string metadataStr(metadata);
    if (metadataStr.empty()) {
        metadataStr = "{}";
    }
    
    // Insert using prepared statement (SQL injection safe!)
    auto stmt = m_db.prepare(
        "INSERT INTO session_tokens "
        "(id, token_hash, user_id, created_at, expires_at, last_ip, user_agent, metadata) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
    );
    
    stmt.bind(1, id);
    stmt.bind(2, tokenHash);
    stmt.bind(3, std::string(userId));
    stmt.bind(4, createdStr);
    stmt.bind(5, expiresStr);
    // C++23: ternary with nullptr/string not allowed; use explicit if
    if (ip.empty()) {
        stmt.bind(6, nullptr);
    } else {
        stmt.bind(6, std::string(ip));
    }
    if (userAgent.empty()) {
        stmt.bind(7, nullptr);
    } else {
        stmt.bind(7, std::string(userAgent));
    }
    stmt.bind(8, metadataStr);
    
    stmt.step();
    stmt.reset();
    
    // Return raw token (ONLY time it's visible!)
    return rawToken;
}

std::optional<SessionToken> SessionTokenStore::validate(std::string_view rawToken) {
    // Hash the provided token
    std::string tokenHash = hashToken(rawToken);
    
    // Lookup by hash using prepared statement
    auto stmt = m_db.prepare(
        "SELECT id, user_id, created_at, expires_at, last_used_at, last_ip, user_agent, "
        "revoked, revoked_at, metadata "
        "FROM session_tokens "
        "WHERE token_hash = ?"
    );
    
    stmt.bind(1, tokenHash);
    
    if (!stmt.step()) {
        // Token not found
        return std::nullopt;
    }
    
    return parseTokenFromStatement(stmt);
}

std::optional<SessionToken> SessionTokenStore::validateAndTouch(
    std::string_view rawToken,
    std::string_view currentIp
) {
    // Hash the provided token
    std::string tokenHash = hashToken(rawToken);
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    // Update last_used_at and last_ip in single query, then return updated token
    auto updateStmt = m_db.prepare(
        "UPDATE session_tokens "
        "SET last_used_at = ?, last_ip = ? "
        "WHERE token_hash = ? AND revoked = 0 AND expires_at > ?"
    );
    
    updateStmt.bind(1, nowStr);
    updateStmt.bind(2, std::string(currentIp));
    updateStmt.bind(3, tokenHash);
    updateStmt.bind(4, nowStr);
    
    updateStmt.step();
    updateStmt.reset();
    
    // Now fetch the updated token
    auto stmt = m_db.prepare(
        "SELECT id, user_id, created_at, expires_at, last_used_at, last_ip, user_agent, "
        "revoked, revoked_at, metadata "
        "FROM session_tokens "
        "WHERE token_hash = ?"
    );
    
    stmt.bind(1, tokenHash);
    
    if (!stmt.step()) {
        return std::nullopt;
    }
    
    return parseTokenFromStatement(stmt);
}

bool SessionTokenStore::revoke(std::string_view rawToken) {
    std::string tokenHash = hashToken(rawToken);
    
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE session_tokens "
        "SET revoked = 1, revoked_at = ? "
        "WHERE token_hash = ?"
    );
    
    stmt.bind(1, nowStr);
    stmt.bind(2, tokenHash);
    
    stmt.step();
    stmt.reset();
    
    // Check if any row was affected
    auto selectStmt = m_db.prepare("SELECT changes()");
    if (selectStmt.step()) {
        int changes = selectStmt.columnInt64(0);
        return changes > 0;
    }
    
    return false;
}

bool SessionTokenStore::revokeById(std::string_view tokenId) {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE session_tokens "
        "SET revoked = 1, revoked_at = ? "
        "WHERE id = ?"
    );
    
    stmt.bind(1, nowStr);
    stmt.bind(2, std::string(tokenId));
    
    stmt.step();
    stmt.reset();
    
    auto selectStmt = m_db.prepare("SELECT changes()");
    if (selectStmt.step()) {
        int changes = selectStmt.columnInt64(0);
        return changes > 0;
    }
    
    return false;
}

size_t SessionTokenStore::revokeAllForUser(std::string_view userId) {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    auto stmt = m_db.prepare(
        "UPDATE session_tokens "
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

std::optional<SessionToken> SessionTokenStore::getById(std::string_view tokenId) {
    auto stmt = m_db.prepare(
        "SELECT id, user_id, created_at, expires_at, last_used_at, last_ip, user_agent, "
        "revoked, revoked_at, metadata "
        "FROM session_tokens "
        "WHERE id = ?"
    );
    
    stmt.bind(1, std::string(tokenId));
    
    if (!stmt.step()) {
        return std::nullopt;
    }
    
    return parseTokenFromStatement(stmt);
}

std::vector<SessionToken> SessionTokenStore::listForUser(
    std::string_view userId,
    bool activeOnly
) {
    std::vector<SessionToken> tokens;
    
    std::string sql;
    if (activeOnly) {
        auto now = Utils::formatIso8601(std::chrono::system_clock::now());
        auto stmt = m_db.prepare(
            "SELECT id, user_id, created_at, expires_at, last_used_at, last_ip, user_agent, "
            "revoked, revoked_at, metadata "
            "FROM session_tokens "
            "WHERE user_id = ? AND revoked = 0 AND expires_at > ? "
            "ORDER BY created_at DESC"
        );
        stmt.bind(1, std::string(userId));
        stmt.bind(2, now);
        
        while (stmt.step()) {
            tokens.push_back(parseTokenFromStatement(stmt));
        }
    } else {
        auto stmt = m_db.prepare(
            "SELECT id, user_id, created_at, expires_at, last_used_at, last_ip, user_agent, "
            "revoked, revoked_at, metadata "
            "FROM session_tokens "
            "WHERE user_id = ? "
            "ORDER BY created_at DESC"
        );
        stmt.bind(1, std::string(userId));
        
        while (stmt.step()) {
            tokens.push_back(parseTokenFromStatement(stmt));
        }
    }
    
    return tokens;
}

size_t SessionTokenStore::cleanup() {
    auto now = std::chrono::system_clock::now();
    std::string nowStr = Utils::formatIso8601(now);
    
    // Count before
    auto countStmt = m_db.prepare("SELECT COUNT(*) FROM session_tokens WHERE expires_at < ?");
    countStmt.bind(1, nowStr);
    
    size_t beforeCount = 0;
    if (countStmt.step()) {
        beforeCount = static_cast<size_t>(countStmt.columnInt64(0));
    }
    
    // Delete expired
    auto deleteStmt = m_db.prepare("DELETE FROM session_tokens WHERE expires_at < ?");
    deleteStmt.bind(1, nowStr);
    deleteStmt.step();
    deleteStmt.reset();
    
    // Also delete permanently revoked tokens older than 30 days
    // (keep revoked tokens for 30 days for audit purposes)
    auto oldRevoked = now - std::chrono::days(30);
    std::string oldRevokedStr = Utils::formatIso8601(oldRevoked);
    
    auto deleteRevokedStmt = m_db.prepare(
        "DELETE FROM session_tokens WHERE revoked = 1 AND revoked_at < ?"
    );
    deleteRevokedStmt.bind(1, oldRevokedStr);
    deleteRevokedStmt.step();
    deleteRevokedStmt.reset();
    
    return beforeCount;
}

size_t SessionTokenStore::count() const {
    auto result = m_db.queryOne("SELECT COUNT(*) FROM session_tokens");
    if (result && !result->empty()) {
        return static_cast<size_t>(std::stoll((*result)[0]));
    }
    return 0;
}

size_t SessionTokenStore::countActive() const {
    auto now = Utils::formatIso8601(std::chrono::system_clock::now());
    
    auto stmt = m_db.prepare(
        "SELECT COUNT(*) FROM session_tokens "
        "WHERE revoked = 0 AND expires_at > ?"
    );
    stmt.bind(1, now);
    
    size_t count = 0;
    if (stmt.step()) {
        count = static_cast<size_t>(stmt.columnInt64(0));
    }
    
    return count;
}

size_t SessionTokenStore::countForUser(std::string_view userId) const {
    auto stmt = m_db.prepare(
        "SELECT COUNT(*) FROM session_tokens WHERE user_id = ?"
    );
    stmt.bind(1, std::string(userId));
    
    size_t count = 0;
    if (stmt.step()) {
        count = static_cast<size_t>(stmt.columnInt64(0));
    }
    
    return count;
}

std::string SessionTokenStore::generateUniqueId() {
    // Retry loop for collision resistance (unlikely but possible)
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::string id = generateShortId(8);
        
        auto stmt = m_db.prepare("SELECT 1 FROM session_tokens WHERE id = ?");
        stmt.bind(1, id);
        
        if (!stmt.step()) {
            return id;  // Unique ID found
        }
    }
    
    // Fallback: use timestamp + random
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch().count();
    return generateShortId(8) + std::to_string(epoch % 1000000);
}

// ============================================================================
// Private Helpers
// ============================================================================

SessionToken SessionTokenStore::parseTokenFromStatement(const DB::Database::Statement& stmt) {
    SessionToken token;
    
    token.id = stmt.columnText(0);
    token.userId = stmt.columnText(1);
    
    // Parse timestamps (stored as ISO 8601)
    auto createdAt = Utils::parseIso8601(stmt.columnText(2));
    if (createdAt) token.createdAt = *createdAt;
    
    auto expiresAt = Utils::parseIso8601(stmt.columnText(3));
    if (expiresAt) token.expiresAt = *expiresAt;
    
    // Optional fields
    if (!stmt.columnIsNull(4)) {
        auto lastUsedAt = Utils::parseIso8601(stmt.columnText(4));
        if (lastUsedAt) token.lastUsedAt = lastUsedAt;
    }
    
    if (!stmt.columnIsNull(5)) {
        token.lastIp = stmt.columnText(5);
    }
    
    if (!stmt.columnIsNull(6)) {
        token.userAgent = stmt.columnText(6);
    }
    
    token.revoked = stmt.columnInt64(7) != 0;
    
    if (!stmt.columnIsNull(8)) {
        auto revokedAt = Utils::parseIso8601(stmt.columnText(8));
        if (revokedAt) token.revokedAt = revokedAt;
    }
    
    token.metadata = stmt.columnText(9);
    
    return token;
}

} // namespace OmniNet::Token