// tools_common/password.cpp - Password Hashing Implementation

#include "password.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace OmniNet::Tools {

constexpr size_t DEFAULT_ITERATIONS = 100000;
constexpr size_t SALT_LENGTH = 16;
constexpr size_t KEY_LENGTH = 32;  // 256 bits

std::string bytesToHex(const unsigned char* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    }
    return oss.str();
}

std::string generateSalt(size_t length) {
    std::vector<unsigned char> salt(length);
    if (RAND_bytes(salt.data(), length) != 1) {
        // Fallback to less secure random if RAND_bytes fails
        for (size_t i = 0; i < length; ++i) {
            salt[i] = rand() % 256;
        }
    }
    return bytesToHex(salt.data(), length);
}

std::string hashPassword(const std::string& password) {
    // Generate salt
    std::vector<unsigned char> salt(SALT_LENGTH);
    if (RAND_bytes(salt.data(), SALT_LENGTH) != 1) {
        for (size_t i = 0; i < SALT_LENGTH; ++i) {
            salt[i] = rand() % 256;
        }
    }
    
    // PBKDF2
    std::vector<unsigned char> hash(KEY_LENGTH);
    if (!PKCS5_PBKDF2_HMAC(
            password.c_str(), password.size(),
            salt.data(), salt.size(),
            DEFAULT_ITERATIONS,
            EVP_sha256(),
            KEY_LENGTH,
            hash.data())) {
        return "";  // Failed
    }
    
    // Format: salt:iterations:hash
    std::ostringstream result;
    result << bytesToHex(salt.data(), SALT_LENGTH);
    result << ":" << DEFAULT_ITERATIONS;
    result << ":" << bytesToHex(hash.data(), KEY_LENGTH);
    
    return result.str();
}

bool verifyPassword(const std::string& password, const std::string& storedHash) {
    // Parse stored hash
    std::istringstream iss(storedHash);
    std::string saltHex, iterationsStr, hashHex;
    
    if (!std::getline(iss, saltHex, ':') ||
        !std::getline(iss, iterationsStr, ':') ||
        !std::getline(iss, hashHex)) {
        return false;
    }
    
    // Parse hex strings back to bytes
    auto hexToBytes = [](const std::string& hex) -> std::vector<unsigned char> {
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteStr = hex.substr(i, 2);
            unsigned char byte = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    };
    
    auto salt = hexToBytes(saltHex);
    auto expectedHash = hexToBytes(hashHex);
    size_t iterations = static_cast<size_t>(std::stoul(iterationsStr));
    
    // Compute hash with same parameters
    std::vector<unsigned char> computedHash(expectedHash.size());
    if (!PKCS5_PBKDF2_HMAC(
            password.c_str(), password.size(),
            salt.data(), salt.size(),
            iterations,
            EVP_sha256(),
            computedHash.size(),
            computedHash.data())) {
        return false;
    }
    
    // Constant-time comparison to prevent timing attacks
    if (computedHash.size() != expectedHash.size()) {
        return false;
    }
    
    unsigned char diff = 0;
    for (size_t i = 0; i < computedHash.size(); ++i) {
        diff |= computedHash[i] ^ expectedHash[i];
    }
    
    return diff == 0;
}

} // namespace OmniNet::Tools
