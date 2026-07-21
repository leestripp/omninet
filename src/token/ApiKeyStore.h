// OmniNet - API Key Store
// Persistent API key storage with SQLite
#ifndef OMNINET_API_KEY_STORE_H
#define OMNINET_API_KEY_STORE_H

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ApiKey.h"
#include "db/Database.h"

namespace OmniNet::Token {

// ============================================================================
// API Key Store
// ============================================================================

class ApiKeyStore {
public:
    // Constructor
    explicit ApiKeyStore(DB::Database& db);
    
    // Disable copy
    ApiKeyStore(const ApiKeyStore&) = delete;
    ApiKeyStore& operator=(const ApiKeyStore&) = delete;
    
    // --- Lifecycle ---
    
    // Initialize database schema
    void initialize();
    
    // --- Key Lifecycle ---
    
    // Create result (raw key shown only once!)
    struct CreateResult {
        std::string rawKey;  // Full key (only shown once!)
        std::string id;      // Public ID (prefix)
    };
    
    // Create new API key with minimal parameters
    // Default prefix: "omn" (configurable via setDefaultPrefix)
    CreateResult create(
        std::string_view userId,
        std::string_view name,
        const std::vector<std::string>& scopes,
        std::chrono::seconds ttl = std::chrono::seconds::zero()  // 0 = never expires
    );
    
    // Create with additional options
    CreateResult create(
        std::string_view userId,
        std::string_view name,
        const std::vector<std::string>& scopes,
        std::chrono::seconds ttl,
        std::string_view ip,
        std::string_view userAgent,
        std::optional<uint64_t> useLimit,
        std::string_view metadata = {}
    );
    
    // --- Validation ---
    
    // Validation result
    struct ValidationResult {
        bool valid;
        std::optional<ApiKey> key;
        std::string error;  // "expired", "revoked", "not_found", "usage_limit", "invalid_format"
        
        static ValidationResult success(ApiKey key) {
            return {true, std::move(key), {}};
        }
        
        static ValidationResult failure(std::string error) {
            return {false, std::nullopt, std::move(error)};
        }
    };
    
    // Validate API key (does not update usage)
    ValidationResult validate(std::string_view rawKey);
    
    // Validate and update usage (increments useCount, updates last_used_at, last_ip)
    ValidationResult validateAndUse(
        std::string_view rawKey,
        std::string_view currentIp
    );
    
    // Check if key has required scope
    bool hasScope(const ApiKey& key, std::string_view requiredScope) const;
    
    // --- Revocation ---
    
    // Revoke by raw key
    bool revoke(std::string_view rawKey);
    
    // Revoke by public ID (key prefix)
    bool revokeById(std::string_view keyId);
    
    // Revoke all keys for a user
    size_t revokeAllForUser(std::string_view userId);
    
    // --- Queries ---
    
    // Get key by public ID
    std::optional<ApiKey> getById(std::string_view keyId);
    
    // List all keys for a user
    std::vector<ApiKey> listForUser(
        std::string_view userId,
        bool activeOnly = false
    );
    
    // --- Cleanup ---
    
    // Remove expired keys
    size_t cleanup();
    
    // --- Statistics ---
    
    size_t count() const;
    size_t countActive() const;
    size_t countForUser(std::string_view userId) const;
    
    // --- Configuration ---
    
    // Set default prefix for new API keys
    void setDefaultPrefix(std::string_view prefix);
    
    const std::string& defaultPrefix() const { return m_defaultPrefix; }

private:
    // Parse API key from database statement
    ApiKey parseKeyFromStatement(const DB::Database::Statement& stmt);
    
    // Generate unique key prefix (8 hex chars + original prefix)
    std::string generateUniquePrefix();
    
    // Parse scopes from JSON array string
    std::vector<std::string> parseScopesFromJson(const std::string& json);
    
    DB::Database& m_db;
    std::string m_defaultPrefix = "omn_sk";
};

} // namespace OmniNet::Token

#endif // OMNINET_API_KEY_STORE_H