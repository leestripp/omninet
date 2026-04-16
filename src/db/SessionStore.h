// OmniNet - Session Store
#ifndef OMNINET_SESSION_STORE_H
#define OMNINET_SESSION_STORE_H

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

#include "Database.h"

namespace OmniNet::DB {

// ============================================================================
// Session Store
// ============================================================================

class SessionStore {
public:
    // Session data structure
    struct Session {
        std::string id;
        std::string userId;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point expires;
        std::string data;  // JSON payload
    };
    
    SessionStore(Database& db);
    
    // --- Lifecycle ---
    
    // Initialize schema
    void initialize();
    
    // --- Session Management ---
    
    // Create new session
    // Returns session ID (base64url encoded)
    std::string create(
        std::string_view userId,
        std::chrono::seconds ttl = std::chrono::hours(24),
        std::string_view data = {}
    );
    
    // Get session by ID
    // Returns nullopt if not found or expired
    std::optional<Session> get(std::string_view sessionId);
    
    // Get session and extend expiry
    std::optional<Session> touch(std::string_view sessionId);
    
    // Delete session
    void remove(std::string_view sessionId);
    
    // Delete all sessions for user
    void removeAllForUser(std::string_view userId);
    
    // --- Cleanup ---
    
    // Remove expired sessions
    size_t cleanup();
    
    // --- Statistics ---
    
    size_t count() const;
    size_t countForUser(std::string_view userId) const;

private:
    Database& m_db;
};

// ============================================================================
// Session Middleware
// ============================================================================

class SessionMiddleware {
public:
    using GetSession = std::function<std::optional<SessionStore::Session>(std::string_view sessionId)>;
    using SaveSession = std::function<void(const SessionStore::Session&)>;
    
    SessionMiddleware(GetSession getSession, SaveSession saveSession);
    
    void setHeaderName(const std::string& name);
    
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);
    
private:
    GetSession m_getSession;
    SaveSession m_saveSession;
    std::string m_headerName = "X-Session-ID";
};

} // namespace OmniNet::DB

#endif // OMNINET_SESSION_STORE_H
