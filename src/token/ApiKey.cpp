// OmniNet - API Key Implementation
#include "ApiKey.h"

#include <algorithm>

namespace OmniNet::Token {

// ============================================================================
// API Key Methods
// ============================================================================

bool ApiKey::isExpired() const {
    if (!expiresAt) return false;  // No expiration set
    return std::chrono::system_clock::now() >= *expiresAt;
}

bool ApiKey::isValid() const {
    if (revoked) return false;
    if (isExpired()) return false;
    
    // Check use limit
    if (useLimit && useCount >= *useLimit) {
        return false;
    }
    
    return true;
}

bool ApiKey::hasScope(std::string_view requiredScope) const {
    // Handle wildcard scope (e.g., "admin:*" matches "admin:users")
    for (const auto& scope : scopes) {
        if (scope == "*") return true;  // Full wildcard
        if (scope == requiredScope) return true;
        
        // Check for prefix wildcard (e.g., "admin:*" matches "admin:users")
        auto colonPos = scope.find(':');
        if (colonPos != std::string::npos) {
            auto prefix = scope.substr(0, colonPos);
            auto requiredPrefix = std::string(requiredScope).substr(0, colonPos);
            if (prefix == requiredPrefix && scope.substr(colonPos + 1) == "*") {
                return true;
            }
        }
    }
    return false;
}

std::optional<uint64_t> ApiKey::remainingUses() const {
    if (!useLimit) return std::nullopt;  // Unlimited
    if (*useLimit <= useCount) return 0;
    return *useLimit - useCount;
}

} // namespace OmniNet::Token