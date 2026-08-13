// OmniNet - OpenSSL Cryptography Implementation (OpenSSL 3.x compatible)
#include "Crypto.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <memory>
#include <vector>

namespace OmniNet {

// ============================================================================
// Global State
// ============================================================================

bool Crypto::s_initialized = false;

void Crypto::initialize() {
    if (!s_initialized) {
        // OpenSSL 3.x auto-initializes, but explicit init is still recommended
        OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
        s_initialized = true;
    }
}

void Crypto::cleanup() {
    if (s_initialized) {
        EVP_cleanup();
        ERR_free_strings();
        CRYPTO_cleanup_all_ex_data();
        s_initialized = false;
    }
}

bool Crypto::isInitialized() {
    return s_initialized;
}

std::string Crypto::lastError() {
    unsigned long error = ERR_get_error();
    if (error == 0) return "";
    return errorString(error);
}

std::string Crypto::errorString(unsigned long error) {
    char buffer[256];
    ERR_error_string_n(error, buffer, sizeof(buffer));
    return buffer;
}

// ============================================================================
// Random
// ============================================================================
std::vector<uint8_t> Crypto::randomBytes(size_t count) {
    std::vector<uint8_t> result(count);
    if (RAND_bytes(result.data(), static_cast<int>(count)) != 1) {
        // Fallback to /dev/urandom if OpenSSL fails
        FILE* f = fopen("/dev/urandom", "rb");
        if (f) {
            // v0.6.0 fix (bug #4): check fread() return value. Previously
            // ignored; a short read would leave part of the buffer
            // uninitialized, and the caller would never know.
            size_t got = fread(result.data(), 1, count, f);
            fclose(f);
            if (got != count) {
                throw std::runtime_error(
                    "Crypto::randomBytes: short read from /dev/urandom");
            }
        } else {
            throw std::runtime_error(
                "Crypto::randomBytes: OpenSSL RAND_bytes failed and "
                "/dev/urandom is unavailable");
        }
    }
    return result;
}
std::string Crypto::randomHex(size_t count) {
    return hexEncode(randomBytes(count));
}
// ============================================================================
// Hashing
// ============================================================================

std::vector<uint8_t> Crypto::sha256(std::span<const uint8_t> data) {
    std::vector<uint8_t> hash(EVP_MAX_MD_SIZE);
    unsigned int hashLen = 0;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 &&
        EVP_DigestFinal_ex(ctx, hash.data(), &hashLen) == 1) {
        hash.resize(hashLen);
    } else {
        hash.clear();
    }
    
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::string Crypto::sha256Hex(std::span<const uint8_t> data) {
    return hexEncode(sha256(data));
}

std::string Crypto::sha256Hex(std::string_view str) {
    return sha256Hex(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.size()
    ));
}

std::vector<uint8_t> Crypto::sha512(std::span<const uint8_t> data) {
    std::vector<uint8_t> hash(EVP_MAX_MD_SIZE);
    unsigned int hashLen = 0;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};
    
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 &&
        EVP_DigestFinal_ex(ctx, hash.data(), &hashLen) == 1) {
        hash.resize(hashLen);
    } else {
        hash.clear();
    }
    
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::string Crypto::sha512Hex(std::span<const uint8_t> data) {
    return hexEncode(sha512(data));
}

// ============================================================================
// HMAC
// ============================================================================

std::vector<uint8_t> Crypto::hmacSha256(
    std::span<const uint8_t> key,
    std::span<const uint8_t> message
) {
    // v0.6.0 fix (bug #5): use OpenSSL 3.0+ EVP_MAC API instead of the
    // deprecated HMAC_CTX_* family (scheduled for removal in OpenSSL 4.0).
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    size_t resultLen = 0;

    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) return {};
    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!ctx) return {};

    char digest[] = "SHA256";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", digest, 0),
        OSSL_PARAM_construct_end()
    };

    bool ok = EVP_MAC_init(ctx, key.data(), key.size(), params) == 1
           && EVP_MAC_update(ctx, message.data(), message.size()) == 1
           && EVP_MAC_final(ctx, result.data(), &resultLen, result.size()) == 1;
    EVP_MAC_CTX_free(ctx);

    if (!ok) {
        result.clear();
    } else {
        result.resize(resultLen);
    }
    return result;
}

std::string Crypto::hmacSha256Hex(
    std::span<const uint8_t> key,
    std::span<const uint8_t> message
) {
    return hexEncode(hmacSha256(key, message));
}

std::string Crypto::hmacSha256(std::string_view key, std::string_view message) {
    auto keyBytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(key.data()), key.size()
    );
    auto msgBytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(message.data()), message.size()
    );
    return hexEncode(hmacSha256(keyBytes, msgBytes));
}

// ============================================================================
// Key Derivation
// ============================================================================

std::vector<uint8_t> Crypto::hkdfSha256(
    std::span<const uint8_t> inputKey,
    std::span<const uint8_t> salt,
    std::span<const uint8_t> info,
    size_t outputLen
) {
    // HKDF-SHA256 using pure HMAC (works with OpenSSL 3.x)
    std::vector<uint8_t> prk = hmacSha256(salt, inputKey);
    std::vector<uint8_t> output(outputLen);
    
    std::vector<uint8_t> t;
    uint8_t counter = 1;
    size_t offset = 0;
    
    while (offset < outputLen) {
        t = hmacSha256(prk, t.empty() ? inputKey : t);
        t.insert(t.end(), info.begin(), info.end());
        t.push_back(counter++);
        
        auto extracted = hmacSha256(prk, t);
        size_t toCopy = std::min(outputLen - offset, extracted.size());
        std::copy(extracted.begin(), extracted.begin() + toCopy, 
                  output.begin() + static_cast<long>(offset));
        offset += toCopy;
        t.clear();
    }
    
    return output;
}

std::vector<uint8_t> Crypto::pbkdf2Sha256(
    std::string_view password,
    std::span<const uint8_t> salt,
    size_t iterations,
    size_t outputLen
) {
    std::vector<uint8_t> output(outputLen);
    
    if (PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        salt.data(), static_cast<int>(salt.size()),
        static_cast<int>(iterations),
        EVP_sha256(),
        static_cast<int>(outputLen),
        output.data()
    ) != 1) {
        return {};
    }
    
    return output;
}
// ============================================================================
// Encoding
// ============================================================================

std::string Crypto::base64Encode(std::span<const uint8_t> data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    bio = BIO_push(b64, bio);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    
    char* ptr = nullptr;
    long len = BIO_get_mem_data(bio, &ptr);
    std::string result(ptr, len);
    
    BIO_free_all(bio);
    return result;
}

std::optional<std::vector<uint8_t>> Crypto::base64Decode(std::string_view encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    bio = BIO_push(b64, bio);
    
    std::vector<uint8_t> result(encoded.size());
    int len = BIO_read(bio, result.data(), static_cast<int>(result.size()));
    
    BIO_free_all(bio);
    
    if (len < 0) return std::nullopt;
    result.resize(len);
    return result;
}
std::string Crypto::hexEncode(std::span<const uint8_t> data) {
    std::string result;
    result.reserve(data.size() * 2);
    
    for (uint8_t b : data) {
        static const char hex[] = "0123456789abcdef";
        result += hex[(b >> 4) & 0xf];
        result += hex[b & 0xf];
    }
    
    return result;
}

std::optional<std::vector<uint8_t>> Crypto::hexDecode(std::string_view hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int byte = 0;
        if (std::sscanf(hex.data() + i, "%02x", &byte) != 1) {
            return std::nullopt;
        }
        result.push_back(static_cast<uint8_t>(byte));
    }
    
    return result;
}

// ============================================================================
// Constant-Time Comparison
// ============================================================================

bool Crypto::constantTimeEqual(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

bool Crypto::constantTimeEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// ============================================================================
// Password Hasher
// ============================================================================
PasswordHasher::PasswordHasher() = default;
PasswordHasher::HashResult PasswordHasher::hash(const std::string& password) const {
    std::vector<uint8_t> salt = Crypto::randomBytes(m_saltLength);
    return hash(password, salt);
}

PasswordHasher::HashResult PasswordHasher::hash(
    const std::string& password,
    std::span<const uint8_t> salt
) const {
    auto result = Crypto::pbkdf2Sha256(password, salt, m_iterations, m_hashLength);
    
    HashResult hashResult;
    hashResult.hash = Crypto::hexEncode(result);
    hashResult.salt = Crypto::hexEncode(std::span<const uint8_t>(salt));
    hashResult.iterations = m_iterations;
    
    return hashResult;
}

bool PasswordHasher::verify(const std::string& password, const HashResult& stored) const {
    auto salt = Crypto::hexDecode(stored.salt);
    if (!salt) return false;
    
    auto computed = Crypto::pbkdf2Sha256(
        password, 
        std::span<const uint8_t>(salt->data(), salt->size()),
        stored.iterations,
        m_hashLength
    );
    
    // Encode computed hash to hex to match stored format
    auto computedHex = Crypto::hexEncode(computed);
    
    return Crypto::constantTimeEqual(
        std::string_view(computedHex),
        std::string_view(stored.hash)
    );
}

bool PasswordHasher::needsRehash(const HashResult& stored) const {
    return stored.iterations != m_iterations;
}

size_t PasswordHasher::iterations() const {
    return m_iterations;
}

void PasswordHasher::setIterations(size_t count) {
    m_iterations = count;
}
size_t PasswordHasher::saltLength() const {
    return m_saltLength;
}

void PasswordHasher::setSaltLength(size_t len) {
    m_saltLength = len;
}
size_t PasswordHasher::hashLength() const {
    return m_hashLength;
}

void PasswordHasher::setHashLength(size_t len) {
    m_hashLength = len;
}

// ============================================================================
// Secure Token
// ============================================================================
std::string SecureToken::generate(size_t length) {
    return Crypto::hexEncode(Crypto::randomBytes(length));
}

std::string SecureToken::generateUrlSafe(size_t length) {
    // Use base64url encoding (URL-safe base64)
    auto bytes = Crypto::randomBytes(length);
    std::string encoded = Crypto::base64Encode(bytes);
    
    // Make URL-safe
    std::replace(encoded.begin(), encoded.end(), '+', '-');
    std::replace(encoded.begin(), encoded.end(), '/', '_');
    
    // Remove padding
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    
    return encoded;
}


// ============================================================================
// Self-Signed Certificate Generation
// ============================================================================

std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
Crypto::generateSelfSignedCert(const std::string& commonName, size_t validityDays) {
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> empty;

    EVP_PKEY_CTX* keyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!keyCtx) return empty;
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen_init(keyCtx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, 2048) <= 0 ||
        EVP_PKEY_keygen(keyCtx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(keyCtx);
        return empty;
    }
    EVP_PKEY_CTX_free(keyCtx);

    X509* x509 = X509_new();
    if (!x509) { EVP_PKEY_free(pkey); return empty; }

    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509),
                     static_cast<long>(std::time(nullptr)));
    X509_gmtime_adj(X509_getm_notBefore(x509), 0);
    X509_gmtime_adj(X509_getm_notAfter(x509),
                    static_cast<long>(validityDays) * 24 * 3600);
    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>(commonName.c_str()),
                               -1, -1, 0);
    X509_set_issuer_name(x509, name);

    bool ok = X509_sign(x509, pkey, EVP_sha256()) > 0;

    std::vector<uint8_t> certPem, keyPem;
    if (ok) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (PEM_write_bio_X509(bio, x509) == 1) {
            BUF_MEM* mem = nullptr;
            BIO_get_mem_ptr(bio, &mem);
            certPem.assign(mem->data, mem->data + mem->length);
        }
        BIO_free(bio);

        bio = BIO_new(BIO_s_mem());
        if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) == 1) {
            BUF_MEM* mem = nullptr;
            BIO_get_mem_ptr(bio, &mem);
            keyPem.assign(mem->data, mem->data + mem->length);
        }
        BIO_free(bio);
    }

    X509_free(x509);
    EVP_PKEY_free(pkey);
    if (certPem.empty() || keyPem.empty()) return empty;
    return {std::move(certPem), std::move(keyPem)};
}

} // namespace OmniNet

