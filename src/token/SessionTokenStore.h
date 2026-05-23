// OmniNet - Session Token Store
// Persistent session token storage with SQLite
#ifndef OMNINET_SESSION_TOKEN_STORE_H
#define OMNINET_SESSION_TOKEN_STORE_H

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SessionToken.h"
#include "db/Database.h"

namespace OmniNet::Token {

// ============================================================================
// Session Token Store
// ============================================================================

class SessionTokenStore {
public:
    // Constructor
    explicit SessionTokenStore(DB::Database& db);
    
    // Disable copy
    SessionTokenStore(const SessionTokenStore&) = delete;
    SessionTokenStore& operator=(const SessionTokenStore&) = delete;
    
    // --- Lifecycle ---
    
    // Initialize database schema
    void initialize();
    
    // --- Token Lifecycle ---
    
    // Create new token
    // Returns raw token (ONLY available at creation time - store securely!)
    std::string create(
        std::string_view userId,
        std::chrono::seconds ttl = std::chrono::hours(24),
        std::string_view metadata = {}
    );
    
    // Create with extended metadata
    std::string create(
        std::string_view userId,
        std::chrono::seconds ttl,
        std::string_view ip,
        std::string_view userAgent,
        std::string_view metadata = {}
    );
    
    // --- Validation ---
    
    // Validate token
    // Returns token data if valid, nullopt otherwise
    // Does NOT update usage statistics
    std::optional<SessionToken> validate(std::string_view rawToken);
    
    // Validate and update usage statistics
    // Updates last_used_at, last_ip
    std::optional<SessionToken> validateAndTouch(
        std::string_view rawToken,
        std::string_view currentIp
    );
    
    // --- Revocation ---
    
    // Revoke a single token by raw token
    // Returns true if token was found and revoked
    bool revoke(std::string_view rawToken);
    
    // Revoke token by token ID (public identifier)
    bool revokeById(std::string_view tokenId);
    
    // Revoke all tokens for a user
    // Returns number of tokens revoked
    size_t revokeAllForUser(std::string_view userId);
    
    // --- Queries ---
    
    // Get token by public ID (without raw token)
    // Note: Raw token is NOT stored, so this can't be used for validation
    std::optional<SessionToken> getById(std::string_view tokenId);
    
    // List all tokens for a user (active only if activeOnly=true)
    std::vector<SessionToken> listForUser(
        std::string_view userId,
        bool activeOnly = false
    );
    
    // --- Cleanup ---
    
    // Remove expired tokens
    // Returns number of tokens removed
    size_t cleanup();
    
    // --- Statistics ---
    
    // Total token count
    size_t count() const;
    
    // Active (non-expired, non-revoked) token count
    size_t countActive() const;
    
    // Token count for specific user
    size_t countForUser(std::string_view userId) const;

private:
    // Internal helpers
    std::string generateUniqueId();
    SessionToken parseTokenFromStatement(const DB::Database::Statement& stmt);
    
    DB::Database& m_db;
};

} // namespace OmniNet::Token

#endif // OMNINET_SESSION_TOKEN_STORE_H