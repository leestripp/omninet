// OmniNet - Password hashing convenience header
#ifndef OMNINET_PASSWORD_H
#define OMNINET_PASSWORD_H

#include <string>

#include "Crypto.h"

namespace OmniNet {

// Get the global password hasher instance
PasswordHasher& getPasswordHasher();

// Convenience functions for password hashing
// Hash format: salt:iterations:hash
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& storedHash);

} // namespace OmniNet

#endif // OMNINET_PASSWORD_H
