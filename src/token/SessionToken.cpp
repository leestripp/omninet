// OmniNet - Session Token Implementation
#include "SessionToken.h"

#include <chrono>

namespace OmniNet::Token {

// ============================================================================
// Session Token Methods
// ============================================================================

bool SessionToken::isExpired() const {
    return std::chrono::system_clock::now() >= expiresAt;
}

bool SessionToken::isValid() const {
    return !revoked && !isExpired();
}

std::chrono::seconds SessionToken::remainingLifetime() const {
    auto now = std::chrono::system_clock::now();
    if (now >= expiresAt) {
        return std::chrono::seconds::zero();
    }
    return std::chrono::duration_cast<std::chrono::seconds>(expiresAt - now);
}

} // namespace OmniNet::Token