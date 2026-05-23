// OmniNet - Session Token
// Secure session token structure
#ifndef OMNINET_SESSION_TOKEN_H
#define OMNINET_SESSION_TOKEN_H

#include <chrono>
#include <optional>
#include <string>

namespace OmniNet::Token {

// ============================================================================
// Session Token
// ============================================================================

struct SessionToken {
    std::string id;                                       // Token ID (public identifier)
    std::string userId;                                   // Associated user ID
    std::chrono::system_clock::time_point createdAt;       // Creation timestamp
    std::chrono::system_clock::time_point expiresAt;      // Expiration timestamp
    std::optional<std::chrono::system_clock::time_point> lastUsedAt;  // Last usage
    std::optional<std::string> lastIp;                     // Last IP address
    std::optional<std::string> userAgent;                  // Last user agent
    bool revoked = false;                                 // Revocation flag
    std::optional<std::chrono::system_clock::time_point> revokedAt;    // Revocation time
    std::string metadata;                                  // JSON metadata
    
    // Check if token is expired
    bool isExpired() const;
    
    // Check if token is valid (not expired, not revoked)
    bool isValid() const;
    
    // Get remaining lifetime
    std::chrono::seconds remainingLifetime() const;
};

} // namespace OmniNet::Token

#endif // OMNINET_SESSION_TOKEN_H