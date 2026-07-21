// OmniNet - Session Store Implementation
#include "utils/Utils.h"
#include "crypto/Crypto.h"
#include "SessionStore.h"

#include <chrono>

namespace OmniNet::DB {

// ============================================================================
// Session Store
// ============================================================================

SessionStore::SessionStore(Database& db) : m_db(db) {}

void SessionStore::initialize() {
    m_db.execute(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            created_at TEXT NOT NULL,
            expires_at TEXT NOT NULL,
            data TEXT
        )
    )");
    
    m_db.execute(R"(
        CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id)
    )");
    
    m_db.execute(R"(
        CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at)
    )");
}

std::string SessionStore::create(
    std::string_view userId,
    std::chrono::seconds ttl,
    std::string_view data
) {
    std::string id = SecureToken::generateUrlSafe(32);
    
    auto now = std::chrono::system_clock::now();
    auto expires = now + ttl;
    
    auto createdStr = Utils::formatRfc2822(now);
    auto expiresStr = Utils::formatRfc2822(expires);
    
    auto stmt = m_db.prepare("INSERT INTO sessions (id, user_id, created_at, expires_at, data) VALUES (?, ?, ?, ?, ?)");
    stmt.bind(1, id);
    stmt.bind(2, userId);
    stmt.bind(3, createdStr);
    stmt.bind(4, expiresStr);
    stmt.bind(5, data);
    stmt.step();
    
    return id;
}

std::optional<SessionStore::Session> SessionStore::get(std::string_view sessionId) {
    auto stmt = m_db.prepare("SELECT id, user_id, created_at, expires_at, data FROM sessions WHERE id = ?");
    stmt.bind(1, sessionId);
    
    if (stmt.step()) {
        Session session;
        session.id = stmt.columnText(0);
        session.userId = stmt.columnText(1);
        session.data = stmt.columnText(4);
        return session;
    }
    
    return std::nullopt;
}

std::optional<SessionStore::Session> SessionStore::touch(std::string_view sessionId) {
    auto session = get(sessionId);
    if (!session) return std::nullopt;
    
    auto now = std::chrono::system_clock::now();
    auto newExpiry = Utils::formatRfc2822(now + std::chrono::hours(24));
    
    auto stmt = m_db.prepare("UPDATE sessions SET expires_at = ? WHERE id = ?");
    stmt.bind(1, newExpiry);
    stmt.bind(2, sessionId);
    stmt.step();
    
    return session;
}

void SessionStore::remove(std::string_view sessionId) {
    auto stmt = m_db.prepare("DELETE FROM sessions WHERE id = ?");
    stmt.bind(1, sessionId);
    stmt.step();
}

void SessionStore::removeAllForUser(std::string_view userId) {
    auto stmt = m_db.prepare("DELETE FROM sessions WHERE user_id = ?");
    stmt.bind(1, userId);
    stmt.step();
}

size_t SessionStore::cleanup() {
    // Count before delete
    auto before = m_db.queryOne("SELECT COUNT(*) FROM sessions");
    size_t beforeCount = before ? std::stoul((*before)[0]) : 0;
    
    m_db.execute("DELETE FROM sessions WHERE expires_at < datetime('now')");
    
    auto after = m_db.queryOne("SELECT COUNT(*) FROM sessions");
    size_t afterCount = after ? std::stoul((*after)[0]) : 0;
    
    return beforeCount - afterCount;
}

size_t SessionStore::count() const {
    auto result = m_db.queryOne("SELECT COUNT(*) FROM sessions");
    return result ? std::stoul((*result)[0]) : 0;
}

size_t SessionStore::countForUser(std::string_view userId) const {
    auto stmt = m_db.prepare("SELECT COUNT(*) FROM sessions WHERE user_id = ?");
    stmt.bind(1, userId);
    
    if (stmt.step()) {
        return static_cast<size_t>(stmt.columnInt64(0));
    }
    return 0;
}

// ============================================================================
// Session Middleware
// ============================================================================

SessionMiddleware::SessionMiddleware(GetSession getSession, SaveSession saveSession)
    : m_getSession(std::move(getSession)), m_saveSession(std::move(saveSession)) {}

void SessionMiddleware::setHeaderName(const std::string& name) {
    m_headerName = name;
}

bool SessionMiddleware::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    (void)resp;
    
    auto sessionId = req.header(m_headerName);
    if (!sessionId) {
        return true;  // No session, continue without it
    }
    
    auto session = m_getSession(*sessionId);
    if (!session) {
        return true;  // Invalid session, continue without it
    }
    
    // Could store session in request context for handlers to access
    return true;
}

} // namespace OmniNet::DB