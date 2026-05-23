// OmniNet - Token Utilities Implementation
#include "TokenUtils.h"
#include "crypto/Crypto.h"
#include "utils/Utils.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace OmniNet::Token {

// Base32 alphabet definition (Crockford's)
static const char* BASE32_ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// ============================================================================
// Generation
// ============================================================================

std::string generateToken(size_t byteLength) {
    auto bytes = generateBytes(byteLength);
    return Utils::bytesToHex(bytes);
}

std::vector<uint8_t> generateBytes(size_t count) {
    return Crypto::randomBytes(count);
}

std::string generateApiKey(std::string_view prefix) {
    // Generate 15 bytes = 120 bits of entropy (base32 encoded = ~24 chars)
    auto bytes = generateBytes(15);
    std::string secret = base32Encode(bytes);
    
    return std::string(prefix) + "_" + secret;
}

std::string generateShortId(size_t length) {
    auto bytes = generateBytes(length);
    return Utils::bytesToHex(bytes);
}

// ============================================================================
// Hashing
// ============================================================================

std::string hashToken(std::string_view token) {
    auto bytes = hashTokenBytes(token);
    return Utils::bytesToHex(bytes);
}

std::vector<uint8_t> hashTokenBytes(std::string_view token) {
    auto span = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(token.data()),
        token.size()
    );
    return Crypto::sha256(span);
}

// ============================================================================
// Extraction
// ============================================================================

std::string extractPrefix(std::string_view apiKey) {
    auto pos = apiKey.find('_');
    if (pos == std::string_view::npos) {
        return {};
    }
    return std::string(apiKey.substr(0, pos));
}

std::string extractSecret(std::string_view apiKey) {
    auto pos = apiKey.find('_');
    if (pos == std::string_view::npos) {
        return {};
    }
    return std::string(apiKey.substr(pos + 1));
}

// ============================================================================
// Validation
// ============================================================================

bool isValidHexFormat(std::string_view token, size_t expectedBytes) {
    // Check length (2 hex chars per byte)
    if (token.size() != expectedBytes * 2) {
        return false;
    }
    return isHexCharset(token);
}

bool isValidApiKeyFormat(std::string_view apiKey, size_t minPrefixLen) {
    // Must contain underscore
    auto pos = apiKey.find('_');
    if (pos == std::string_view::npos) {
        return false;
    }
    
    // Prefix must be at least minPrefixLen characters
    if (pos < minPrefixLen) {
        return false;
    }
    
    // Check prefix contains only alphanumeric and underscores
    auto prefix = apiKey.substr(0, pos);
    for (char c : prefix) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    
    // Secret (after underscore) must be at least 16 characters
    auto secret = apiKey.substr(pos + 1);
    if (secret.size() < 16) {
        return false;
    }
    
    // Secret should be alphanumeric (base32-like)
    for (char c : secret) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    
    return true;
}

bool isHexCharset(std::string_view hex) {
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Comparison
// ============================================================================

bool secureCompare(std::string_view a, std::string_view b) {
    auto aBytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(a.data()),
        a.size()
    );
    auto bBytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(b.data()),
        b.size()
    );
    return secureCompareBytes(aBytes, bBytes);
}

bool secureCompareBytes(
    std::span<const uint8_t> a,
    std::span<const uint8_t> b
) {
    return Crypto::constantTimeEqual(a, b);
}

// ============================================================================
// Base32 Encoding (Crockford's Base32)
// ============================================================================

std::string base32Encode(std::span<const uint8_t> data) {
    std::string result;
    result.reserve((data.size() * 8 + 4) / 5);  // ~1.6x expansion
    
    size_t bits = 0;
    size_t value = 0;
    
    for (uint8_t byte : data) {
        value = (value << 8) | byte;
        bits += 8;
        
        while (bits >= 5) {
            bits -= 5;
            result += BASE32_ALPHABET[(value >> bits) & 0x1F];
        }
    }
    
    // Handle remaining bits
    if (bits > 0) {
        result += BASE32_ALPHABET[(value << (5 - bits)) & 0x1F];
    }
    
    return result;
}

std::optional<std::vector<uint8_t>> base32Decode(std::string_view encoded) {
    // Normalize: uppercase, remove hyphens, treat '=' as padding
    std::string normalized;
    normalized.reserve(encoded.size());
    
    for (char c : encoded) {
        if (c == '=') break;  // Stop at padding
        if (c == '-') continue;  // Skip hyphens
        normalized += std::toupper(static_cast<unsigned char>(c));
    }
    
    if (normalized.empty()) {
        return std::nullopt;
    }
    
    // Convert characters to 5-bit values
    std::vector<size_t> values;
    values.reserve(normalized.size());
    
    for (char c : normalized) {
        int v = c - '0';
        if (v >= 0 && v <= 9) {
            values.push_back(v);
        } else if (c >= 'A' && c <= 'Z') {
            // Crockford's maps: A=10, B=11, ... but skips I, L, O, U
            // For simplicity, we use standard base32
            v = c - 'A' + 10;
            // Handle special mappings: O->0, I->1, L->1
            if (c == 'O' || c == 'o') v = 0;
            else if (c == 'I' || c == 'i' || c == 'L' || c == 'l') v = 1;
            values.push_back(v);
        } else {
            return std::nullopt;  // Invalid character
        }
    }
    
    // Decode 5-bit values to bytes
    std::vector<uint8_t> result;
    size_t bits = 0;
    size_t value = 0;
    
    for (size_t v : values) {
        value = (value << 5) | v;
        bits += 5;
        
        while (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((value >> bits) & 0xFF));
        }
    }
    
    return result;
}

} // namespace OmniNet::Token