// OmniNet - Configuration system
#ifndef OMNINET_CONFIG_H
#define OMNINET_CONFIG_H

#include <chrono>
#include <string>
#include <optional>
#include <filesystem>

namespace OmniNet::Config {

// ============================================================================
// Global Configuration
// ============================================================================

class Config {
public:
    static Config& instance();
    
    // Prevent copying
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // --- Paths ---
    
    // Get/set data directory (~/.omninet by default)
    const std::filesystem::path& dataDir() const;
    void setDataDir(const std::filesystem::path& path);
    
    // Resolve path relative to data directory
    std::filesystem::path resolve(const std::string& filename) const;
    
    // --- Logging ---
    
    // Default log level
    LogLevel logLevel() const;
    void setLogLevel(LogLevel level);
    
    // Log file path
    const std::optional<std::string>& logFile() const;
    void setLogFile(const std::string& path);
    
    // --- Server Defaults ---
    
    // Default port
    uint16_t defaultPort() const;
    void setDefaultPort(uint16_t port);
    
    // Default request timeout
    std::chrono::seconds defaultRequestTimeout() const;
    void setDefaultRequestTimeout(std::chrono::seconds timeout);
    
    // --- Threading ---
    
    // Default worker thread count (0 = auto-detect)
    size_t defaultWorkerCount() const;
    void setDefaultWorkerCount(size_t count);
    
    // --- TLS Defaults ---
    
    // Default TLS certificate path
    const std::optional<std::string>& defaultTlsCert() const;
    void setDefaultTlsCert(const std::string& path);
    
    // Default TLS key path
    const std::optional<std::string>& defaultTlsKey() const;
    void setDefaultTlsKey(const std::string& path);
    
    // --- Database ---
    
    // Default database path
    const std::filesystem::path& databasePath() const;
    void setDatabasePath(const std::filesystem::path& path);
    
    // --- Environment ---
    
    // Load config from environment variables
    void loadFromEnvironment();
    
    // Load config from file (JSON or env-format)
    void loadFromFile(const std::string& path);
    
private:
    Config();
    ~Config() = default;
    
    std::filesystem::path m_dataDir;
    std::optional<std::string> m_logFile;
    LogLevel m_logLevel = LogLevel::INFO;
    uint16_t m_defaultPort = 8080;
    std::chrono::seconds m_defaultRequestTimeout{30};
    size_t m_defaultWorkerCount = 0;  // 0 = auto
    std::optional<std::string> m_defaultTlsCert;
    std::optional<std::string> m_defaultTlsKey;
    std::filesystem::path m_databasePath;
};

// ============================================================================
// Server Configuration Structure
// ============================================================================

struct ServerConfig {
    std::string address = "0.0.0.0";
    uint16_t port = 8080;
    size_t workerCount = 0;  // 0 = auto
    std::chrono::seconds requestTimeout{30};
    std::chrono::seconds keepAliveTimeout{5};
    size_t maxRequestSize = 1024 * 1024;  // 1MB
    
    // TLS
    bool tlsEnabled = false;
    std::string tlsCertFile;
    std::string tlsKeyFile;
    bool tlsVerifyClient = false;
    
    // CORS
    bool corsEnabled = false;
    std::string corsAllowedOrigins;
    std::string corsAllowedMethods;
    std::string corsAllowedHeaders;
    std::chrono::seconds corsMaxAge{86400};
};

// ============================================================================
// WebSocket Server Configuration
// ============================================================================

struct WebSocketConfig {
    std::chrono::seconds pingInterval{30};
    std::chrono::seconds pongTimeout{10};
    size_t maxMessageSize = 1024 * 1024;  // 1MB
    size_t maxFrameSize = 65536;  // 64KB
    size_t maxConnections = 1024;
    bool autoFragment = true;
    size_t fragmentSize = 4096;
};

// ============================================================================
// Database Configuration
// ============================================================================

struct DatabaseConfig {
    std::string path;
    int busyTimeoutMs = 5000;
    bool foreignKeysEnabled = true;
    bool WALModeEnabled = true;
    size_t cacheSizeKb = 2000;
};

// ============================================================================
// Dashboard Configuration
// ============================================================================

struct DashboardConfig {
    std::string urlPath = "/omninet";
    std::string apiPath = "/omninet/api";
    bool enabled = true;
    bool allowRemoteAccess = false;
    std::vector<std::string> allowedIpPrefixes = {"127.", "::1"};
    int sessionTtlSeconds = 3600;
    int maxLoginAttempts = 5;
    std::chrono::seconds lockoutDuration{300};
};

// ============================================================================
// Intrusion Detection Configuration
// ============================================================================

struct IntrusionConfig {
    bool enabled = true;
    bool autoBlock = true;
    std::chrono::minutes blockDuration{30};
    size_t maxFailedAttempts = 10;
    std::chrono::minutes attemptWindow{15};
    size_t maxLogEntries = 10000;
};

} // namespace OmniNet::Config

#endif // OMNINET_CONFIG_H