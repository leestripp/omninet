// OmniNet - Password Hashing
// This file provides convenience wrappers around Crypto::PasswordHasher
#include "Password.h"
#include "utils/Utils.h"

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
    if (parts.size() != 3) return false;
    
    PasswordHasher::HashResult result;
    result.salt = parts[0];
    result.iterations = static_cast<size_t>(std::atoi(parts[1].c_str()));
    result.hash = parts[2];
    
    return getPasswordHasher().verify(password, result);
}

} // namespace OmniNet
