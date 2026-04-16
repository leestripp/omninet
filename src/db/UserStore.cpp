// OmniNet - User Store Implementation
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
    
    m_db.executef(
        "INSERT INTO users (username, password_hash, password_salt, role, created_at) VALUES ('%s', '%s', '%s', '%s', '%s')",
        username.c_str(),
        hashResult.hash.c_str(),
        hashResult.salt.c_str(),
        actualRole.c_str(),
        now.c_str()
    );
    
    return m_db.lastInsertRowId();
}

std::optional<UserStore::User> UserStore::verify(
    const std::string& username,
    const std::string& password
) {
    auto result = m_db.queryOne(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE username = '" + username + "'"
    );
    
    if (!result) return std::nullopt;
    
    User user;
    if (result->size() >= 6) {
        user.id = std::stoll((*result)[0]);
        user.username = (*result)[1];
        user.passwordHash = (*result)[2];
        user.passwordSalt = (*result)[3];
        user.role = (*result)[4];
    }
    
    // Verify password
    PasswordHasher::HashResult stored;
    stored.hash = user.passwordHash;
    stored.salt = user.passwordSalt;
    
    if (!m_hasher.verify(password, stored)) {
        return std::nullopt;
    }
    
    return user;
}

std::optional<UserStore::User> UserStore::getById(int64_t id) {
    auto result = m_db.queryOne(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE id = " + std::to_string(id)
    );
    
    if (!result) return std::nullopt;
    
    User user;
    if (result->size() >= 6) {
        user.id = std::stoll((*result)[0]);
        user.username = (*result)[1];
        user.passwordHash = (*result)[2];
        user.passwordSalt = (*result)[3];
        user.role = (*result)[4];
    }
    
    return user;
}

std::optional<UserStore::User> UserStore::getByUsername(const std::string& username) {
    auto result = m_db.queryOne(
        "SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users WHERE username = '" + username + "'"
    );
    
    if (!result) return std::nullopt;
    
    User user;
    if (result->size() >= 6) {
        user.id = std::stoll((*result)[0]);
        user.username = (*result)[1];
        user.passwordHash = (*result)[2];
        user.passwordSalt = (*result)[3];
        user.role = (*result)[4];
    }
    
    return user;
}

std::vector<UserStore::User> UserStore::list() {
    std::vector<User> users;
    
    m_db.query("SELECT id, username, password_hash, password_salt, role, created_at, last_login_at FROM users", 
        [&users](const std::vector<std::string>& row) {
            User user;
            if (row.size() >= 6) {
                user.id = std::stoll(row[0]);
                user.username = row[1];
                user.passwordHash = row[2];
                user.passwordSalt = row[3];
                user.role = row[4];
                users.push_back(user);
            }
            return true;
        });
    
    return users;
}

void UserStore::remove(int64_t id) {
    m_db.execute("DELETE FROM users WHERE id = " + std::to_string(id));
}

void UserStore::setRole(int64_t id, const std::string& role) {
    m_db.executef(
        "UPDATE users SET role = '%s' WHERE id = %lld",
        role.c_str(),
        id
    );
}

bool UserStore::isAdmin(int64_t id) {
    auto result = m_db.queryOne(
        "SELECT role FROM users WHERE id = " + std::to_string(id)
    );
    return result && (*result)[0] == "admin";
}

void UserStore::updatePassword(int64_t id, const std::string& newPassword) {
    auto hashResult = m_hasher.hash(newPassword);
    
    m_db.executef(
        "UPDATE users SET password_hash = '%s', password_salt = '%s' WHERE id = %lld",
        hashResult.hash.c_str(),
        hashResult.salt.c_str(),
        id
    );
}

bool UserStore::needsRehash(const User& user) {
    PasswordHasher::HashResult stored;
    stored.hash = user.passwordHash;
    stored.salt = user.passwordSalt;
    return m_hasher.needsRehash(stored);
}

void UserStore::recordLogin(int64_t id) {
    auto now = Utils::formatRfc2822(std::chrono::system_clock::now());
    m_db.executef(
        "UPDATE users SET last_login_at = '%s' WHERE id = %lld",
        now.c_str(),
        id
    );
}

std::optional<std::chrono::system_clock::time_point> UserStore::lastLogin(int64_t id) {
    auto result = m_db.queryOne(
        "SELECT last_login_at FROM users WHERE id = " + std::to_string(id)
    );
    
    if (!result || result->empty()) return std::nullopt;
    
    return Utils::parseRfc2822((*result)[0]);
}

size_t UserStore::count() const {
    auto result = m_db.queryOne("SELECT COUNT(*) FROM users");
    return result ? std::stoul((*result)[0]) : 0;
}

size_t UserStore::countByRole(const std::string& role) const {
    auto result = m_db.queryOne(
        "SELECT COUNT(*) FROM users WHERE role = '" + role + "'"
    );
    return result ? std::stoul((*result)[0]) : 0;
}

// ============================================================================
// Auth Middleware
// ============================================================================

AuthMiddleware::AuthMiddleware(VerifyUserCallback verifyUser, GetUserCallback getUser)
    : m_verifyUser(std::move(verifyUser)), m_getUser(std::move(getUser)) {}

bool AuthMiddleware::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    (void)req; (void)resp;
    // TODO: Implement auth middleware
    return true;
}

std::optional<UserStore::User> AuthMiddleware::getAuthenticatedUser(const HTTP::Request& req) {
    (void)req;
    // TODO: Extract credentials and verify
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