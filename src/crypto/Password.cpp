// OmniNet - Password Hashing
// This file provides convenience wrappers around Crypto::PasswordHasher
#include "Password.h"
#include "utils/Utils.h"
#include "crypto/Crypto.h"

namespace OmniNet {
using Utils::split;

PasswordHasher& getPasswordHasher() {
    static PasswordHasher instance;
    return instance;
}

std::string hashPassword(const std::string& password) {
    auto result = getPasswordHasher().hash(password);
    return result.salt + ":" + std::to_string(result.iterations) + ":" + result.hash;
}

bool verifyPassword(const std::string& password, const std::string& storedHash) {
    auto parts = Utils::split(storedHash, ':');
    if (parts.size() != 3) {
        // Try legacy format (no iterations field, just salt:hash)
        // Legacy format: 32-char salt + 64-char hash (no iterations)
        if (parts.size() == 2 && parts[0].length() == 32 && parts[1].length() == 64) {
            PasswordHasher::HashResult result;
            result.salt = parts[0];
            result.iterations = 100000; // Default iterations for legacy hashes
            result.hash = parts[1];
            return getPasswordHasher().verify(password, result);
        }
        return false;
    }
    
    // Verify salt length is valid (32 hex chars = 16 bytes, or 64 hex chars = 32 bytes)
    size_t saltLen = parts[0].length();
    if (saltLen != 32 && saltLen != 64) {
        return false;
    }
    
    PasswordHasher::HashResult result;
    result.salt = parts[0];
    result.iterations = static_cast<size_t>(std::atoi(parts[1].c_str()));
    result.hash = parts[2];
    
    return getPasswordHasher().verify(password, result);
}

} // namespace OmniNet
