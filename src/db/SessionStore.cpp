// OmniNet - Session Store Implementation
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
    
    m_db.executef(
        "INSERT INTO sessions (id, user_id, created_at, expires_at, data) VALUES ('%s', '%s', '%s', '%s', '%s')",
        id.c_str(),
        std::string(userId).c_str(),
        createdStr.c_str(),
        expiresStr.c_str(),
        std::string(data).c_str()
    );
    
    return id;
}

std::optional<SessionStore::Session> SessionStore::get(std::string_view sessionId) {
    auto result = m_db.queryOne(
        "SELECT id, user_id, created_at, expires_at, data FROM sessions WHERE id = '" + std::string(sessionId) + "'"
    );
    
    if (!result) return std::nullopt;
    
    Session session;
    if (result->size() >= 5) {
        session.id = (*result)[0];
        session.userId = (*result)[1];
        session.data = (*result)[4];
    }
    
    return session;
}

std::optional<SessionStore::Session> SessionStore::touch(std::string_view sessionId) {
    auto session = get(sessionId);
    if (!session) return std::nullopt;
    
    auto now = std::chrono::system_clock::now();
    m_db.executef(
        "UPDATE sessions SET expires_at = '%s' WHERE id = '%s'",
        Utils::formatRfc2822(now + std::chrono::hours(24)).c_str(),
        std::string(sessionId).c_str()
    );
    
    return session;
}

void SessionStore::remove(std::string_view sessionId) {
    m_db.executef(
        "DELETE FROM sessions WHERE id = '%s'",
        std::string(sessionId).c_str()
    );
}

void SessionStore::removeAllForUser(std::string_view userId) {
    m_db.executef(
        "DELETE FROM sessions WHERE user_id = '%s'",
        std::string(userId).c_str()
    );
}

size_t SessionStore::cleanup() {
    auto now = std::chrono::system_clock::now();
    auto nowStr = Utils::formatRfc2822(now);
    
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
    auto result = m_db.queryOne(
        "SELECT COUNT(*) FROM sessions WHERE user_id = '" + std::string(userId) + "'"
    );
    return result ? std::stoul((*result)[0]) : 0;
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