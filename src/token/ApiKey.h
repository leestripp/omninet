// OmniNet - API Key
// Secure API key structure
#ifndef OMNINET_API_KEY_H
#define OMNINET_API_KEY_H

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OmniNet::Token {

// ============================================================================
// API Key
// ============================================================================

struct ApiKey {
    std::string id;                                       // Public key ID (prefix)
    std::string userId;                                   // Owner user ID
    std::string name;                                     // Human-readable name
    std::vector<std::string> scopes;                     // Permission scopes
    std::chrono::system_clock::time_point createdAt;     // Creation timestamp
    std::optional<std::chrono::system_clock::time_point> expiresAt;  // Expiration
    std::optional<std::chrono::system_clock::time_point> lastUsedAt;  // Last usage
    std::optional<std::string> lastIp;                   // Last IP address
    bool revoked = false;                                 // Revocation flag
    std::optional<std::chrono::system_clock::time_point> revokedAt;   // Revocation time
    uint64_t useCount = 0;                                // Total usage count
    std::optional<uint64_t> useLimit;                     // Max uses (null = unlimited)
    std::string metadata;                                  // JSON metadata
    
    // Check if key is expired
    bool isExpired() const;
    
    // Check if key is valid (not expired, not revoked, within use limit)
    bool isValid() const;
    
    // Check if key has a specific scope
    bool hasScope(std::string_view requiredScope) const;
    
    // Get remaining uses (null if unlimited)
    std::optional<uint64_t> remainingUses() const;
};

} // namespace OmniNet::Token

#endif // OMNINET_API_KEY_H