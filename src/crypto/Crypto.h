// OmniNet - OpenSSL Cryptography wrapper
#ifndef OMNINET_CRYPTO_H
#define OMNINET_CRYPTO_H

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
struct ssl_st;
struct ssl_ctx_st;
struct evp_pkey_st;
struct x509_st;

namespace OmniNet {

// ============================================================================
// TLS Mode
// ============================================================================

enum class TLSMode {
    CLIENT,
    SERVER
};

// ============================================================================
// TLS Verification Level
// ============================================================================

enum class TLSVerify {
    NONE,          // No verification
    PEER,          // Verify certificate
    PEER_STRICT,   // Verify and require certificate
    CLIENT_ONCE    // Client cert, but optional
};

// ============================================================================
// Crypto Core (OpenSSL wrapper)
// ============================================================================

class Crypto {
public:
    // --- Global Initialization ---
    // Must be called once before using any Crypto functionality
    static void initialize();
    
    // Must be called once after done using Crypto
    static void cleanup();
    
    // Check if initialized
    static bool isInitialized();
    
    // --- Error Handling ---
    
    // Get last error message
    static std::string lastError();
    
    // Get error message for specific error code
    static std::string errorString(unsigned long error);
    
    // --- Random ---
    
    // Generate cryptographically secure random bytes
    static std::vector<uint8_t> randomBytes(size_t count);
    
    // Generate random hex string
    static std::string randomHex(size_t count);
    
    // --- Hashing ---
    
    // SHA-256 hash
    static std::vector<uint8_t> sha256(std::span<const uint8_t> data);
    static std::string sha256Hex(std::span<const uint8_t> data);
    static std::string sha256Hex(std::string_view str);
    
    // SHA-512 hash
    static std::vector<uint8_t> sha512(std::span<const uint8_t> data);
    static std::string sha512Hex(std::span<const uint8_t> data);
    
    // --- HMAC ---
    
    // HMAC-SHA256
    static std::vector<uint8_t> hmacSha256(
        std::span<const uint8_t> key,
        std::span<const uint8_t> message
    );
    static std::string hmacSha256Hex(
        std::span<const uint8_t> key,
        std::span<const uint8_t> message
    );
    
    // HMAC-SHA256 with string inputs
    static std::string hmacSha256(
        std::string_view key,
        std::string_view message
    );
    
    // --- Key Derivation ---
    
    // HKDF-SHA256
    static std::vector<uint8_t> hkdfSha256(
        std::span<const uint8_t> inputKey,
        std::span<const uint8_t> salt,
        std::span<const uint8_t> info,
        size_t outputLen
    );
    
    // PBKDF2-SHA256 (for password hashing)
    static std::vector<uint8_t> pbkdf2Sha256(
        std::string_view password,
        std::span<const uint8_t> salt,
        size_t iterations,
        size_t outputLen
    );
    
    // --- Encoding ---
    
    // Base64 encode
    static std::string base64Encode(std::span<const uint8_t> data);
    
    // Base64 decode
    static std::optional<std::vector<uint8_t>> base64Decode(std::string_view encoded);
    
    // Hex encode
    static std::string hexEncode(std::span<const uint8_t> data);
    
    // Hex decode
    static std::optional<std::vector<uint8_t>> hexDecode(std::string_view hex);
    
    // --- Key Generation ---
    
    // Generate RSA key pair
    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> generateRsaKeyPair(
        size_t bits = 2048
    );
    
    // Generate EC key pair (P-256)
    static std::vector<uint8_t> generateEcPrivateKey();
    
    // Get public key from EC private key
    static std::vector<uint8_t> getEcPublicKey(std::span<const uint8_t> privateKey);
    
    // --- Certificate Utilities ---
    
    // Load certificate from PEM file
    static std::shared_ptr<x509_st> loadCertificate(const std::string& path);
    
    // Load private key from PEM file
    static std::shared_ptr<evp_pkey_st> loadPrivateKey(const std::string& path);
    
    // Generate self-signed certificate
    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> generateSelfSignedCert(
        const std::string& commonName,
        size_t validityDays = 365
    );
    
    // Get certificate common name
    static std::optional<std::string> getCertificateCN(x509_st* cert);
    
    // --- Constant-Time Comparison ---
    
    // Compare two byte sequences in constant time
    static bool constantTimeEqual(
        std::span<const uint8_t> a,
        std::span<const uint8_t> b
    );
    
    // Constant-time string comparison
    static bool constantTimeEqual(std::string_view a, std::string_view b);

private:
    static bool s_initialized;
    static unsigned long s_errorBufferSize;
};

// ============================================================================
// Password Hasher
// ============================================================================

class PasswordHasher {
public:
    struct HashResult {
        std::string hash;      // The computed hash (hex)
        std::string salt;       // The salt used (hex)
        size_t iterations;     // Number of iterations
    };
    
    // Default parameters
    static constexpr size_t DEFAULT_ITERATIONS = 100000;
    static constexpr size_t DEFAULT_SALT_LEN = 32;
    static constexpr size_t DEFAULT_HASH_LEN = 32;
    
    PasswordHasher();
    
    // Hash a password
    HashResult hash(const std::string& password) const;
    
    // Hash with specific salt
    HashResult hash(const std::string& password, std::span<const uint8_t> salt) const;
    
    // Verify password against stored hash
    bool verify(const std::string& password, const HashResult& stored) const;
    
    // Check if hash needs rehashing (e.g., different algorithm)
    bool needsRehash(const HashResult& stored) const;
    
    // Get/set parameters
    size_t iterations() const;
    void setIterations(size_t count);
    
    size_t saltLength() const;
    void setSaltLength(size_t len);
    
    size_t hashLength() const;
    void setHashLength(size_t len);

private:
    size_t m_iterations = DEFAULT_ITERATIONS;
    size_t m_saltLength = DEFAULT_SALT_LEN;
    size_t m_hashLength = DEFAULT_HASH_LEN;
};

// ============================================================================
// Secure Token Generator
// ============================================================================

class SecureToken {
public:
    // Generate a cryptographically secure random token
    static std::string generate(size_t length = 32);
    
    // Generate URL-safe token (base64url encoded)
    static std::string generateUrlSafe(size_t length = 32);
};

} // namespace OmniNet

#endif // OMNINET_CRYPTO_H
