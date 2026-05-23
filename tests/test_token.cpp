// OmniNet Token Utilities Tests
#include "token/TokenUtils.h"
#include "token/SessionToken.h"
#include "token/ApiKey.h"
#include "db/Database.h"
#include <iostream>
#include <unordered_set>
#include <cstring>

using namespace OmniNet::Token;

int passed = 0;
int failed = 0;

#define ASSERT_TRUE(cond) do { if (!(cond)) { std::cout << "FAIL (" << #cond << ")\n"; failed++; } } while(0)
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cout << "FAIL: " << #a << " != " << #b << " (got: " << (a) << ")\n"; failed++; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { std::cout << "FAIL: " << #a << " == " << #b << "\n"; failed++; } } while(0)
#define ASSERT_STREQ(a, b) do { if (strcmp((a), (b)) != 0) { std::cout << "FAIL: \"" << (a) << "\" != \"" << (b) << "\"\n"; failed++; } } while(0)
#define RUN(name) do { std::cout << "  " #name "... "; test_##name(); std::cout << "PASS\n"; passed++; } while(0)

// ============================================================================
// TokenUtils Tests
// ============================================================================

void test_generate_token_length() {
    // Default length (32 bytes = 64 hex chars)
    auto token = generateToken();
    ASSERT_EQ(token.size(), 64);
    
    // Custom length
    auto token16 = generateToken(16);
    ASSERT_EQ(token16.size(), 32);
    
    auto token8 = generateToken(8);
    ASSERT_EQ(token8.size(), 16);
}

void test_generate_token_charset() {
    auto token = generateToken();
    
    // Check all characters are hex
    for (char c : token) {
        ASSERT_TRUE(
            (c >= '0' && c <= '9') || 
            (c >= 'a' && c <= 'f')
        );
    }
}

void test_generate_token_uniqueness() {
    std::unordered_set<std::string> tokens;
    
    // Generate 1000 tokens and ensure all are unique
    for (int i = 0; i < 1000; i++) {
        auto token = generateToken();
        ASSERT_TRUE(tokens.find(token) == tokens.end());
        tokens.insert(token);
    }
    
    ASSERT_EQ(tokens.size(), 1000);
}

void test_generate_bytes() {
    auto bytes = generateBytes(32);
    ASSERT_EQ(bytes.size(), 32u);
    
    auto empty = generateBytes(0);
    ASSERT_EQ(empty.size(), 0u);
}

void test_generate_api_key_format() {
    auto key = generateApiKey("test");
    
    // Should contain underscore
    ASSERT_TRUE(key.find('_') != std::string::npos);
    
    // Should start with prefix
    ASSERT_TRUE(key.substr(0, 5) == "test_");
    
    // Total length should be prefix + 1 + ~24 (base32 of 15 bytes)
    ASSERT_TRUE(key.size() >= 20);
}

void test_generate_api_key_uniqueness() {
    std::unordered_set<std::string> keys;
    
    for (int i = 0; i < 100; i++) {
        auto key = generateApiKey("test");
        ASSERT_TRUE(keys.find(key) == keys.end());
        keys.insert(key);
    }
    
    ASSERT_EQ(keys.size(), 100);
}

void test_hash_token_deterministic() {
    std::string input = "test_token_value";
    
    auto hash1 = hashToken(input);
    auto hash2 = hashToken(input);
    
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash1.size(), 64);  // SHA-256 = 32 bytes = 64 hex chars
}

void test_hash_token_different() {
    auto hash1 = hashToken("token1");
    auto hash2 = hashToken("token2");
    
    ASSERT_NE(hash1, hash2);
}

void test_hash_token_bytes() {
    auto bytes = hashTokenBytes("test");
    ASSERT_EQ(bytes.size(), 32u);  // SHA-256 output
    
    // Verify it's deterministic
    auto bytes2 = hashTokenBytes("test");
    ASSERT_EQ(bytes.size(), bytes2.size());
    ASSERT_TRUE(bytes == bytes2);
}

void test_extract_prefix() {
    // Extracts BEFORE first underscore
    ASSERT_EQ(extractPrefix("omn_sk_a3f8b2c1"), "omn");
    ASSERT_EQ(extractPrefix("test_key"), "test");
    ASSERT_EQ(extractPrefix("single"), "");
    ASSERT_EQ(extractPrefix(""), "");
}

void test_extract_secret() {
    // Extracts AFTER first underscore
    ASSERT_EQ(extractSecret("omn_sk_a3f8b2c1d4e5f678"), "sk_a3f8b2c1d4e5f678");
    ASSERT_EQ(extractSecret("test_key"), "key");
    ASSERT_EQ(extractSecret("single"), "");
    ASSERT_EQ(extractSecret(""), "");
}

void test_is_valid_hex_format() {
    // Generate valid 32-byte hex token and verify
    std::string valid64 = generateToken(32);  // 64 hex chars
    ASSERT_EQ(valid64.size(), 64u);
    ASSERT_TRUE(isValidHexFormat(valid64, 32));
    
    // Wrong length - truncate to 63 chars
    std::string wrongLen = valid64.substr(0, 63);
    ASSERT_EQ(wrongLen.size(), 63u);
    ASSERT_FALSE(isValidHexFormat(wrongLen, 32));
    
    // Invalid characters - replace first char with 'g'
    std::string invalid = valid64;
    invalid[0] = 'g';
    ASSERT_FALSE(isValidHexFormat(invalid, 32));
}

void test_is_valid_api_key_format() {
    // Generate a valid API key format and test with it
    std::string validKey = generateApiKey("test");
    // Valid key should pass format check (our generator creates valid format)
    ASSERT_TRUE(isValidApiKeyFormat(validKey, 3));
    
    // No underscore at all
    ASSERT_FALSE(isValidApiKeyFormat("testnounderscorevalu"));
    
    // Prefix too short (less than 3 chars before underscore)
    ASSERT_FALSE(isValidApiKeyFormat("ab_keyvalue1234567"));
    
    // Secret too short (less than 16 chars after underscore)
    ASSERT_FALSE(isValidApiKeyFormat("test_short"));
    
    // Secret with underscore fails (must be alphanumeric)
    ASSERT_FALSE(isValidApiKeyFormat("test_has_underscore123"));
}

void test_is_hex_charset() {
    ASSERT_TRUE(isHexCharset("0123456789abcdefABCDEF"));
    ASSERT_FALSE(isHexCharset("0123456789xyz"));
    ASSERT_TRUE(isHexCharset(""));
    ASSERT_TRUE(isHexCharset("deadbeef"));
}

void test_secure_compare_equal() {
    std::string a = "test_string_123";
    std::string b = "test_string_123";
    
    ASSERT_TRUE(secureCompare(a, b));
}

void test_secure_compare_different_length() {
    std::string a = "short";
    std::string b = "longer_string";
    
    ASSERT_FALSE(secureCompare(a, b));
}

void test_secure_compare_different_content() {
    std::string a = "test_string";
    std::string b = "test_string!";
    
    ASSERT_FALSE(secureCompare(a, b));
}

void test_generate_short_id() {
    auto id = generateShortId(8);
    ASSERT_EQ(id.size(), 16);  // 8 bytes = 16 hex chars
    
    auto id4 = generateShortId(4);
    ASSERT_EQ(id4.size(), 8);  // 4 bytes = 8 hex chars
}

void test_base32_encode() {
    // Empty
    std::vector<uint8_t> empty;
    ASSERT_EQ(base32Encode(empty), "");
    
    // Simple test - verify encoding is non-empty for non-empty input
    std::vector<uint8_t> test = {'f', 'o', 'o'};
    auto encoded = base32Encode(test);
    ASSERT_TRUE(!encoded.empty());
    // Should be uppercase alphanumeric
    for (char c : encoded) {
        if (c != '=') {  // Ignore padding
            ASSERT_TRUE(
                (c >= '0' && c <= '9') || 
                (c >= 'A' && c <= 'Z')
            );
        }
    }
}

void test_base32_encode_decode_roundtrip() {
    std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    auto encoded = base32Encode(original);
    auto decoded = base32Decode(encoded);
    
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->size(), original.size());
    
    // Compare elements
    for (size_t i = 0; i < original.size(); i++) {
        ASSERT_EQ((*decoded)[i], original[i]);
    }
}

void test_base32_decode_invalid() {
    // Invalid characters
    auto result = base32Decode("!!!invalid!!!");
    ASSERT_FALSE(result.has_value());
}

// ============================================================================
// Session Token Tests
// ============================================================================

void test_session_token_is_expired() {
    SessionToken token;
    
    // Future expiration - not expired
    token.expiresAt = std::chrono::system_clock::now() + std::chrono::hours(1);
    ASSERT_FALSE(token.isExpired());
    
    // Past expiration - expired
    token.expiresAt = std::chrono::system_clock::now() - std::chrono::hours(1);
    ASSERT_TRUE(token.isExpired());
}

void test_session_token_is_valid() {
    SessionToken token;
    token.expiresAt = std::chrono::system_clock::now() + std::chrono::hours(1);
    
    // Not revoked, not expired - valid
    token.revoked = false;
    ASSERT_TRUE(token.isValid());
    
    // Revoked - not valid
    token.revoked = true;
    ASSERT_FALSE(token.isValid());
}

void test_session_token_remaining_lifetime() {
    SessionToken token;
    
    token.expiresAt = std::chrono::system_clock::now() + std::chrono::hours(1);
    auto remaining = token.remainingLifetime();
    ASSERT_TRUE(remaining.count() > 0);
    ASSERT_TRUE(remaining.count() <= 3600);
    
    // Already expired
    token.expiresAt = std::chrono::system_clock::now() - std::chrono::hours(1);
    remaining = token.remainingLifetime();
    ASSERT_EQ(remaining.count(), 0);
}

// ============================================================================
// API Key Tests
// ============================================================================

void test_api_key_is_expired() {
    ApiKey key;
    
    // No expiration set
    ASSERT_FALSE(key.isExpired());
    
    // Future expiration
    key.expiresAt = std::chrono::system_clock::now() + std::chrono::hours(1);
    ASSERT_FALSE(key.isExpired());
    
    // Past expiration
    key.expiresAt = std::chrono::system_clock::now() - std::chrono::hours(1);
    ASSERT_TRUE(key.isExpired());
}

void test_api_key_is_valid() {
    ApiKey key;
    key.expiresAt = std::chrono::system_clock::now() + std::chrono::hours(1);
    
    // Not revoked, not expired, no limit - valid
    key.revoked = false;
    ASSERT_TRUE(key.isValid());
    
    // Revoked
    key.revoked = true;
    ASSERT_FALSE(key.isValid());
}

void test_api_key_has_scope() {
    ApiKey key;
    key.scopes = {"read", "write:orders", "admin:users"};
    
    // Exact match
    ASSERT_TRUE(key.hasScope("read"));
    ASSERT_TRUE(key.hasScope("write:orders"));
    
    // No match
    ASSERT_FALSE(key.hasScope("delete"));
    
    // Wildcard scope
    key.scopes = {"*"};
    ASSERT_TRUE(key.hasScope("anything"));
    ASSERT_TRUE(key.hasScope("admin:users"));
}

void test_api_key_remaining_uses() {
    ApiKey key;
    
    // No limit
    ASSERT_FALSE(key.remainingUses().has_value());
    
    // With limit
    key.useLimit = 100;
    key.useCount = 50;
    auto remaining = key.remainingUses();
    ASSERT_TRUE(remaining.has_value());
    ASSERT_EQ(*remaining, 50u);
    
    // At limit
    key.useCount = 100;
    remaining = key.remainingUses();
    ASSERT_TRUE(remaining.has_value());
    ASSERT_EQ(*remaining, 0u);
}

// ============================================================================
// Run All Tests
// ============================================================================

int main() {
    std::cout << "\n=== TokenUtils Tests ===\n";
    RUN(generate_token_length);
    RUN(generate_token_charset);
    RUN(generate_token_uniqueness);
    RUN(generate_bytes);
    RUN(generate_api_key_format);
    RUN(generate_api_key_uniqueness);
    RUN(hash_token_deterministic);
    RUN(hash_token_different);
    RUN(hash_token_bytes);
    RUN(extract_prefix);
    RUN(extract_secret);
    RUN(is_valid_hex_format);
    RUN(is_valid_api_key_format);
    RUN(is_hex_charset);
    RUN(secure_compare_equal);
    RUN(secure_compare_different_length);
    RUN(secure_compare_different_content);
    RUN(generate_short_id);
    RUN(base32_encode);
    RUN(base32_encode_decode_roundtrip);
    RUN(base32_decode_invalid);
    
    std::cout << "\n=== SessionToken Tests ===\n";
    RUN(session_token_is_expired);
    RUN(session_token_is_valid);
    RUN(session_token_remaining_lifetime);
    
    std::cout << "\n=== ApiKey Tests ===\n";
    RUN(api_key_is_expired);
    RUN(api_key_is_valid);
    RUN(api_key_has_scope);
    RUN(api_key_remaining_uses);
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed, 0 skipped\n";
    
    return failed > 0 ? 1 : 0;
}