// OmniNet - User Store
#ifndef OMNINET_USER_STORE_H
#define OMNINET_USER_STORE_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "Database.h"
#include "crypto/Crypto.h"

namespace OmniNet::DB {

// Forward declarations
namespace HTTP {
class Request;
class Response;
}

// ============================================================================
// User Store
// ============================================================================

class UserStore {
public:
    // User data structure
    struct User {
        int64_t id = 0;
        std::string username;
        std::string passwordHash;
        std::string passwordSalt;
        std::string role;  // "admin" or "user"
        std::chrono::system_clock::time_point created;
        std::optional<std::chrono::system_clock::time_point> lastLogin;
    };
    
    UserStore(Database& db);
    
    // --- Lifecycle ---
    
    // Initialize schema
    void initialize();
    
    // --- User Management ---
    
    // Create user
    // Password is hashed automatically
    int64_t createUser(
        const std::string& username,
        const std::string& password,
        const std::string& role = "user"
    );
    
    // Verify username and password
    // Returns user on success, nullopt on failure
    std::optional<User> verify(
        const std::string& username,
        const std::string& password
    );
    
    // Get user by ID
    std::optional<User> getById(int64_t id);
    
    // Get user by username
    std::optional<User> getByUsername(const std::string& username);
    
    // List all users
    std::vector<User> list();
    
    // Delete user
    void remove(int64_t id);
    
    // --- Role Management ---
    
    // Update user role
    void setRole(int64_t id, const std::string& role);
    
    // Check if user is admin
    bool isAdmin(int64_t id);
    
    // --- Password Management ---
    
    // Update password
    void updatePassword(int64_t id, const std::string& newPassword);
    
    // Check if password needs rehashing
    bool needsRehash(const User& user);
    
    // --- Login Tracking ---
    
    // Record successful login
    void recordLogin(int64_t id);
    
    // Get last login time
    std::optional<std::chrono::system_clock::time_point> lastLogin(int64_t id);
    
    // --- Statistics ---
    
    size_t count() const;
    size_t countByRole(const std::string& role) const;

private:
    Database& m_db;
    PasswordHasher m_hasher;
};

// ============================================================================
// Auth Middleware
// ============================================================================

class AuthMiddleware {
public:
    using VerifyUser = std::function<std::optional<UserStore::User>(
        const std::string& username,
        const std::string& password
    )>;
    using GetUser = std::function<std::optional<UserStore::User>(int64_t id)>;
    
    AuthMiddleware(VerifyUser verifyUser, GetUser getUser);
    
    // Extract auth from request
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);
    
    // Get authenticated user from request
    std::optional<UserStore::User> getAuthenticatedUser(const HTTP::Request& req);
    
    // Get user ID from request (for route handlers)
    std::optional<int64_t> getUserId(const HTTP::Request& req);
    
private:
    VerifyUser m_verifyUser;
    GetUser m_getUser;
};

// ============================================================================
// Role-Based Access Control
// ============================================================================

class RBAC {
public:
    RBAC(GetUser getUser);
    
    // Check if user has role
    bool hasRole(int64_t userId, const std::string& role);
    
    // Check if user is admin
    bool isAdmin(int64_t userId);
    
    // Require specific role
    void requireRole(int64_t userId, const std::string& role);
    
    // Require admin privileges
    void requireAdmin(int64_t userId);

private:
    GetUser m_getUser;
};

// ============================================================================
// HTTP Basic Auth
// ============================================================================

class BasicAuth {
public:
    using ValidateCredentials = std::function<std::optional<std::string>(
        const std::string& username,
        const std::string& password
    )>;
    
    BasicAuth(ValidateCredentials validate);
    
    // WWW-Authenticate realm
    void setRealm(const std::string& realm);
    
    // Parse Authorization header
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);
    
    // Get credentials from header
    static std::optional<std::pair<std::string, std::string>> parseBasicAuth(
        std::string_view header
    );

private:
    ValidateCredentials m_validate;
    std::string m_realm = "Restricted";
};

} // namespace OmniNet::DB

#endif // OMNINET_USER_STORE_H
