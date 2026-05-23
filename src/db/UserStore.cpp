// OmniNet - User Store Implementation
#include <cctype>
#include "utils/Utils.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "UserStore.h"

namespace OmniNet::DB {

// ============================================================================
// User Store
// ============================================================================

UserStore::UserStore(Database& db) : m_db(db) {}

void UserStore::initialize() {
    m_db.execute(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            password_salt TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'user',
            created_at TEXT NOT NULL,
            last_login_at TEXT
        )
    )");
    
    m_db.execute(R"(
        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)
    )");
}

int64_t UserStore::createUser(
    const std::string& username,
    const std::string& password,
    const std::string& role
) {
    // Hash password
    auto hashResult = m_hasher.hash(password);
    
    // Check if this is the first user (becomes admin)
    bool isFirstUser = (count() == 0);
    std::string actualRole = isFirstUser ? "admin" : role;
    
    auto now = Utils::formatRfc2822(std::chrono::system_clock::now());
    
    auto stmt = m_db.prepare(
        "INSERT INTO users (username, password_hash, password_salt, role, created_at) VALUES (?, ?, ?, ?, ?)"
    );
    stmt.bind(1, username);
    stmt.bind(2, hashResult.hash);
    stmt.bind(3, hashResult.salt);
    stmt.bind(4, actualRole);
    stmt.bind(5, now);
    stmt.step();
    
    return m_db.lastInsertRowId();
}

std::optional<UserStore::User> UserStore::verify(
    const std::string& username,
    const std::string& password
) {
    auto stmt = m_db.prepare(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE username = ?"
    );
    stmt.bind(1, username);
    
    if (stmt.step()) {
        User user;
        user.id = stmt.columnInt64(0);
        user.username = stmt.columnText(1);
        user.passwordHash = stmt.columnText(2);
        user.passwordSalt = stmt.columnText(3);
        user.role = stmt.columnText(4);
        
        // Verify password
        PasswordHasher::HashResult stored;
        stored.hash = user.passwordHash;
        stored.salt = user.passwordSalt;
        
        if (!m_hasher.verify(password, stored)) {
            return std::nullopt;
        }
        
        return user;
    }
    
    return std::nullopt;
}

std::optional<UserStore::User> UserStore::getById(int64_t id) {
    auto stmt = m_db.prepare(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE id = ?"
    );
    stmt.bind(1, id);
    
    if (stmt.step()) {
        User user;
        user.id = stmt.columnInt64(0);
        user.username = stmt.columnText(1);
        user.passwordHash = stmt.columnText(2);
        user.passwordSalt = stmt.columnText(3);
        user.role = stmt.columnText(4);
        return user;
    }
    
    return std::nullopt;
}

std::optional<UserStore::User> UserStore::getByUsername(const std::string& username) {
    auto stmt = m_db.prepare(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE username = ?"
    );
    stmt.bind(1, username);
    
    if (stmt.step()) {
        User user;
        user.id = stmt.columnInt64(0);
        user.username = stmt.columnText(1);
        user.passwordHash = stmt.columnText(2);
        user.passwordSalt = stmt.columnText(3);
        user.role = stmt.columnText(4);
        return user;
    }
    
    return std::nullopt;
}

std::vector<UserStore::User> UserStore::list() {
    std::vector<User> users;
    
    auto stmt = m_db.prepare(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users"
    );
    
    while (stmt.step()) {
        User user;
        user.id = stmt.columnInt64(0);
        user.username = stmt.columnText(1);
        user.passwordHash = stmt.columnText(2);
        user.passwordSalt = stmt.columnText(3);
        user.role = stmt.columnText(4);
        users.push_back(user);
    }
    
    return users;
}

void UserStore::remove(int64_t id) {
    auto stmt = m_db.prepare("DELETE FROM users WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
}

void UserStore::setRole(int64_t id, const std::string& role) {
    auto stmt = m_db.prepare("UPDATE users SET role = ? WHERE id = ?");
    stmt.bind(1, role);
    stmt.bind(2, id);
    stmt.step();
}

bool UserStore::isAdmin(int64_t id) {
    auto stmt = m_db.prepare("SELECT role FROM users WHERE id = ?");
    stmt.bind(1, id);
    
    if (stmt.step()) {
        return stmt.columnText(0) == "admin";
    }
    return false;
}

void UserStore::updatePassword(int64_t id, const std::string& newPassword) {
    auto hashResult = m_hasher.hash(newPassword);
    
    auto stmt = m_db.prepare("UPDATE users SET password_hash = ?, password_salt = ? WHERE id = ?");
    stmt.bind(1, hashResult.hash);
    stmt.bind(2, hashResult.salt);
    stmt.bind(3, id);
    stmt.step();
}

bool UserStore::needsRehash(const User& user) {
    PasswordHasher::HashResult stored;
    stored.hash = user.passwordHash;
    stored.salt = user.passwordSalt;
    return m_hasher.needsRehash(stored);
}

void UserStore::recordLogin(int64_t id) {
    auto now = Utils::formatRfc2822(std::chrono::system_clock::now());
    
    auto stmt = m_db.prepare("UPDATE users SET last_login_at = ? WHERE id = ?");
    stmt.bind(1, now);
    stmt.bind(2, id);
    stmt.step();
}

std::optional<std::chrono::system_clock::time_point> UserStore::lastLogin(int64_t id) {
    auto stmt = m_db.prepare("SELECT last_login_at FROM users WHERE id = ?");
    stmt.bind(1, id);
    
    if (stmt.step() && !stmt.columnIsNull(0)) {
        return Utils::parseRfc2822(stmt.columnText(0));
    }
    return std::nullopt;
}

size_t UserStore::count() const {
    auto result = m_db.queryOne("SELECT COUNT(*) FROM users");
    return result ? std::stoul((*result)[0]) : 0;
}

size_t UserStore::countByRole(const std::string& role) const {
    auto stmt = m_db.prepare("SELECT COUNT(*) FROM users WHERE role = ?");
    stmt.bind(1, role);
    
    if (stmt.step()) {
        return static_cast<size_t>(stmt.columnInt64(0));
    }
    return 0;
}

// ============================================================================
// Auth Middleware
// ============================================================================

AuthMiddleware::AuthMiddleware(VerifyUserCallback verifyUser, GetUserCallback getUser)
    : m_verifyUser(std::move(verifyUser)), m_getUser(std::move(getUser)) {}

bool AuthMiddleware::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    auto user = getAuthenticatedUser(req);
    if (user) {
        return true;  // Authentication successful
    }
    
    // Check for required authentication
    auto authHeader = req.header("Authorization");
    if (authHeader && 
        (authHeader->substr(0, 6) == "Bearer" || authHeader->substr(0, 5) == "Basic")) {
        // Client provided credentials but they weren't valid
        resp = HTTP::Response::unauthorized("Bearer realm=\"Protected\"");
        return false;
    }
    
    // No credentials provided
    resp = HTTP::Response::unauthorized("Bearer realm=\"Protected\"");
    return false;
}

std::optional<UserStore::User> AuthMiddleware::getAuthenticatedUser(const HTTP::Request& req) {
    // Try Bearer token first
    auto authHeader = req.header("Authorization");
    if (authHeader && authHeader->substr(0, 7) == "Bearer ") {
        std::string token(authHeader->substr(7));
        // Trim whitespace
        while (!token.empty() && std::isspace(token.back())) token.pop_back();
        
        if (m_verifyUser) {
            auto result = m_verifyUser(token, "");
            if (result) {
                if (m_getUser) {
                    return m_getUser(result->id);
                }
                return result;
            }
        }
    }
    
    // Try Basic authentication
    if (authHeader && authHeader->substr(0, 6) == "Basic ") {
        std::string encoded(authHeader->substr(6));
        // Basic auth is base64 encoded username:password
        // This would need base64 decoding - for now just try direct match
        if (m_verifyUser) {
            // Assuming token is the password for Basic auth
            auto result = m_verifyUser("", encoded);
            if (result) {
                if (m_getUser) {
                    return m_getUser(result->id);
                }
                return result;
            }
        }
    }
    
    return std::nullopt;
}

std::optional<int64_t> AuthMiddleware::getUserId(const HTTP::Request& req) {
    auto user = getAuthenticatedUser(req);
    return user ? std::optional<int64_t>(user->id) : std::nullopt;
}

// ============================================================================
// RBAC
// ============================================================================

RBAC::RBAC(GetUserCallback getUser) : m_getUser(std::move(getUser)) {}

bool RBAC::hasRole(int64_t userId, const std::string& role) {
    auto user = m_getUser(userId);
    return user && user->role == role;
}

bool RBAC::isAdmin(int64_t userId) {
    return hasRole(userId, "admin");
}

void RBAC::requireRole(int64_t userId, const std::string& role) {
    if (!hasRole(userId, role)) {
        throw RBAC::PermissionException("Required role: " + role);
    }
}

void RBAC::requireAdmin(int64_t userId) {
    if (!isAdmin(userId)) {
        throw RBAC::PermissionException("Admin access required");
    }
}

// ============================================================================
// Basic Auth
// ============================================================================

BasicAuth::BasicAuth(ValidateCredentials validate)
    : m_validate(std::move(validate)) {}

void BasicAuth::setRealm(const std::string& realm) {
    m_realm = realm;
}

bool BasicAuth::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    auto auth = req.header("Authorization");
    if (!auth) {
        resp.setStatus(HTTP::StatusCode::UNAUTHORIZED);
        resp.setHeader("WWW-Authenticate", "Basic realm=\"" + m_realm + "\"");
        return false;
    }
    
    auto creds = parseBasicAuth(*auth);
    if (!creds) {
        resp = HTTP::Response::unauthorized("Basic");
        return false;
    }
    
    auto username = m_validate(creds->first, creds->second);
    if (!username) {
        resp = HTTP::Response::unauthorized("Basic");
        return false;
    }
    
    return true;
}

std::optional<std::pair<std::string, std::string>> BasicAuth::parseBasicAuth(std::string_view header) {
    if (!header.starts_with("Basic ")) {
        return std::nullopt;
    }
    
    auto encoded = Utils::trim(header.substr(6));
    auto decoded = Utils::base64DecodeToString(std::string(encoded));
    if (!decoded) return std::nullopt;
    
    auto colon = decoded->find(':');
    if (colon == std::string::npos) return std::nullopt;
    
    return std::make_pair(
        decoded->substr(0, colon),
        decoded->substr(colon + 1)
    );
}

} // namespace OmniNet::DB