// OmniNet - Token Utilities
// Secure token generation, hashing, and validation utilities
#ifndef OMNINET_TOKEN_UTILS_H
#define OMNINET_TOKEN_UTILS_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OmniNet::Token {

// ============================================================================
// Token Utilities
// ============================================================================

// --- Generation ---

// Generate secure random token (hex encoded, lowercase)
// Default: 32 bytes = 64 hex chars
std::string generateToken(size_t byteLength = 32);

// Generate random bytes (binary)
std::vector<uint8_t> generateBytes(size_t count);

// Generate API key with custom prefix
// Format: <prefix>_<random_base32>
// Length: prefix_len + 1 + 24 = ~33 chars
std::string generateApiKey(std::string_view prefix);

// --- Hashing ---

// Hash token using SHA-256 (hex encoded)
std::string hashToken(std::string_view token);

// Hash token using SHA-256 (raw bytes)
std::vector<uint8_t> hashTokenBytes(std::string_view token);

// --- Extraction ---

// Extract prefix from API key (before first underscore)
// Returns empty string if no underscore found
std::string extractPrefix(std::string_view apiKey);

// Extract secret from API key (after first underscore)
// Returns empty string if no underscore found
std::string extractSecret(std::string_view apiKey);

// --- Validation ---

// Check if token is valid hex format
bool isValidHexFormat(std::string_view token, size_t expectedBytes = 32);

// Check if API key is valid format (prefix_secret)
bool isValidApiKeyFormat(std::string_view apiKey, size_t minPrefixLen = 3);

// Check if hex string contains only valid hex characters
bool isHexCharset(std::string_view hex);

// --- Comparison ---

// Constant-time comparison to prevent timing attacks
// Returns true if strings are equal
bool secureCompare(std::string_view a, std::string_view b);

// Compare byte sequences in constant time
bool secureCompareBytes(
    std::span<const uint8_t> a,
    std::span<const uint8_t> b
);

// --- API Key Helpers ---

// Generate a short ID for API keys (8 hex chars)
std::string generateShortId(size_t length = 8);

// Encode bytes to base32 (Crockford's base32, URL-safe)
std::string base32Encode(std::span<const uint8_t> data);

// Decode base32 string to bytes
std::optional<std::vector<uint8_t>> base32Decode(std::string_view encoded);

} // namespace OmniNet::Token

#endif // OMNINET_TOKEN_UTILS_H