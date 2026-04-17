// tools_common/password.h - Password Hashing for CLI Tools
// Uses PBKDF2-SHA256 for password hashing (compatible with OmniNet library)

#ifndef OMNINET_TOOLS_PASSWORD_H
#define OMNINET_TOOLS_PASSWORD_H

#include <string>

namespace OmniNet::Tools {

// Hash a password using PBKDF2-SHA256
// Returns format: salt:iterations:hash
std::string hashPassword(const std::string& password);

// Verify a password against a stored hash
bool verifyPassword(const std::string& password, const std::string& storedHash);

// Generate a random salt
std::string generateSalt(size_t length = 16);

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_PASSWORD_H
