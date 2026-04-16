// OmniNet - User Store
#ifndef OMNINET_USER_STORE_H
#define OMNINET_USER_STORE_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "Database.h"
#include "crypto/Crypto.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace OmniNet::DB {

// ============================================================================
// User Store
// ============================================================================

class UserStore {
public:
    struct User {
        int64_t id = 0;
        std::string username;
        std::string passwordHash;
        std::string passwordSalt;
        std::string role;
        std::chrono::system_clock::time_point created;
        std::optional<std::chrono::system_clock::time_point> lastLogin;
    };
    
    UserStore(Database& db);
    void initialize();
    int64_t createUser(const std::string& username, const std::string& password, const std::string& role = "user");
    std::optional<User> verify(const std::string& username, const std::string& password);
    std::optional<User> getById(int64_t id);
    std::optional<User> getByUsername(const std::string& username);
    std::vector<User> list();
    void remove(int64_t id);
    void setRole(int64_t id, const std::string& role);
    bool isAdmin(int64_t id);
    void updatePassword(int64_t id, const std::string& newPassword);
    bool needsRehash(const User& user);
    void recordLogin(int64_t id);
    std::optional<std::chrono::system_clock::time_point> lastLogin(int64_t id);
    size_t count() const;
    size_t countByRole(const std::string& role) const;

private:
    Database& m_db;
    PasswordHasher m_hasher;
};

// ============================================================================
// Auth Middleware
// ============================================================================

using VerifyUserCallback = std::function<std::optional<UserStore::User>(const std::string&, const std::string&)>;
using GetUserCallback = std::function<std::optional<UserStore::User>(int64_t)>;

class AuthMiddleware {
public:
    AuthMiddleware(VerifyUserCallback verifyUser, GetUserCallback getUser);
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);
    std::optional<UserStore::User> getAuthenticatedUser(const HTTP::Request& req);
    std::optional<int64_t> getUserId(const HTTP::Request& req);
    
private:
    VerifyUserCallback m_verifyUser;
    GetUserCallback m_getUser;
};

// ============================================================================
// RBAC
// ============================================================================

class RBAC {
public:
    RBAC(GetUserCallback getUser);
    bool hasRole(int64_t userId, const std::string& role);
    bool isAdmin(int64_t userId);
    void requireRole(int64_t userId, const std::string& role);
    void requireAdmin(int64_t userId);
    struct PermissionException : public std::exception {
        std::string message;
        PermissionException(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    };

private:
    GetUserCallback m_getUser;
};

// ============================================================================
// Basic Auth
// ============================================================================

class BasicAuth {
public:
    using ValidateCredentials = std::function<std::optional<std::string>(const std::string&, const std::string&)>;
    BasicAuth(ValidateCredentials validate);
    void setRealm(const std::string& realm);
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);
    static std::optional<std::pair<std::string, std::string>> parseBasicAuth(std::string_view header);

private:
    ValidateCredentials m_validate;
    std::string m_realm = "Restricted";
};

} // namespace OmniNet::DB

#endif // OMNINET_USER_STORE_H
