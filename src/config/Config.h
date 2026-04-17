// OmniNet - Configuration system
#ifndef OMNINET_CONFIG_H
#define OMNINET_CONFIG_H

#include <chrono>
#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <functional>

#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace OmniNet {

// ============================================================================
// Global Configuration (inside OmniNet namespace)
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
    
    // Config file path (~/.omninet/config.json)
    std::filesystem::path configFile() const;
    
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
    
    // Default keep-alive timeout
    std::chrono::seconds defaultKeepAliveTimeout() const;
    void setDefaultKeepAliveTimeout(std::chrono::seconds timeout);
    
    // Default max request size
    size_t defaultMaxRequestSize() const;
    void setDefaultMaxRequestSize(size_t size);
    
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
    
    // Default TLS certificate/key file paths (~/.omninet/certs/)
    // Returns path to default certificate if exists, empty otherwise
    std::filesystem::path defaultTlsCertPath() const;
    
    // Returns path to default private key if exists, empty otherwise
    std::filesystem::path defaultTlsKeyPath() const;
    
    // --- CORS Defaults ---
    
    // Default CORS enabled
    bool defaultCorsEnabled() const;
    void setDefaultCorsEnabled(bool enabled);
    
    // Default CORS allowed origins
    const std::string& defaultCorsOrigins() const;
    void setDefaultCorsOrigins(const std::string& origins);
    
    // Default CORS allowed methods
    const std::string& defaultCorsMethods() const;
    void setDefaultCorsMethods(const std::string& methods);
    
    // Default CORS allowed headers
    const std::string& defaultCorsHeaders() const;
    void setDefaultCorsHeaders(const std::string& headers);
    
    // Default CORS max age
    std::chrono::seconds defaultCorsMaxAge() const;
    void setDefaultCorsMaxAge(std::chrono::seconds maxAge);
    
    // --- Database ---
    
    // Default database path
    const std::filesystem::path& databasePath() const;
    void setDatabasePath(const std::filesystem::path& path);
    
    // --- Loading ---
    
    // Load config from environment variables
    void loadFromEnvironment();
    
    // Load config from file (JSON)
    void loadFromFile(const std::string& path);
    
    // Load from default config file (~/.omninet/config.json)
    // Creates with defaults if file doesn't exist
    void load();
    
    // --- Validation ---
    
    // Validate all configuration values
    bool validate() const;
    std::vector<std::string> validationErrors() const;
    
    // --- Hot Reload ---
    
    // Reload configuration from file
    bool reload();
    
    // Watch config file for changes and auto-reload
    void startFileWatcher();
    void stopFileWatcher();
    
    // Register callback for config change notifications
    using ChangeCallback = std::function<void(const std::string& path)>;
    void onChange(ChangeCallback callback);
    
    // --- Serialization ---
    
    // Dump current config as JSON string
    std::string toJson() const;
    
    // Save current config to file
    bool save(const std::string& path = "");
    
    // Create default config file
    void createDefaultConfig();
    
    // --- Generic Access ---
    
    // Get a value from the JSON config by dot-notation path
    // Examples: "server.port", "tls.enabled", "cors.origins"
    // Returns defaultValue if path doesn't exist
    template<typename T>
    T get(const std::string& path, T defaultValue) const;
    
    // Get a string value from config
    std::string getString(const std::string& path, const std::string& defaultValue = "") const;
    
    // Get an integer value from config
    int getInt(const std::string& path, int defaultValue = 0) const;
    
    // Get a boolean value from config
    bool getBool(const std::string& path, bool defaultValue = false) const;
    
    // Check if a path exists in config
    bool has(const std::string& path) const;
    
private:
    Config();
    ~Config();
    
    // File watcher thread function
    void fileWatcherLoop();
    
    // Internal load from JSON
    void loadFromJson(const json& j);
    
    // Convert current state to JSON
    json toJsonInternal() const;
    
    std::filesystem::path m_dataDir;
    std::optional<std::string> m_logFile;
    LogLevel m_logLevel = LogLevel::INFO;
    uint16_t m_defaultPort = 8080;
    std::chrono::seconds m_defaultRequestTimeout{30};
    std::chrono::seconds m_defaultKeepAliveTimeout{5};
    size_t m_defaultMaxRequestSize = 1024 * 1024;  // 1MB
    size_t m_defaultWorkerCount = 0;  // 0 = auto
    std::optional<std::string> m_defaultTlsCert;
    std::optional<std::string> m_defaultTlsKey;
    std::filesystem::path m_databasePath;
    
    // CORS defaults
    bool m_defaultCorsEnabled = false;
    std::string m_defaultCorsOrigins = "*";
    std::string m_defaultCorsMethods = "GET,POST,PUT,DELETE";
    std::string m_defaultCorsHeaders = "Content-Type,Authorization";
    std::chrono::seconds m_defaultCorsMaxAge{86400};
    
    // Hot reload
    std::thread m_watcherThread;
    std::atomic<bool> m_watcherRunning{false};
    std::vector<ChangeCallback> m_changeCallbacks;
    std::filesystem::file_time_type m_configLastModified;
    
    mutable std::mutex m_mutex;
    mutable std::vector<std::string> m_validationErrors;
    
    // JSON storage for generic config access
    json m_jsonConfig;
};

// ============================================================================
// Template Implementation
// ============================================================================

template<typename T>
T Config::get(const std::string& path, T defaultValue) const {
    try {
        const json* current = &m_jsonConfig;
        std::stringstream ss(path);
        std::string segment;
        
        while (std::getline(ss, segment, '.')) {
            if (!current->contains(segment)) {
                return defaultValue;
            }
            current = &((*current)[segment]);
        }
        
        return current->get<T>();
    } catch (...) {
        return defaultValue;
    }
}

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
    
    // Serialization
    json toJson() const;
    static ServerConfig fromJson(const json& j);
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
    
    // Serialization
    json toJson() const;
    static WebSocketConfig fromJson(const json& j);
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
    
    // Serialization
    json toJson() const;
    static DatabaseConfig fromJson(const json& j);
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
    
    // Serialization
    json toJson() const;
    static DashboardConfig fromJson(const json& j);
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
    
    // Serialization
    json toJson() const;
    static IntrusionConfig fromJson(const json& j);
};

// ============================================================================
// Logging Configuration
// ============================================================================

struct LoggingConfig {
    std::string level = "INFO";
    std::string file = "omninet.log";
    bool toConsole = true;
    bool toFile = true;
    size_t maxSizeMb = 10;
    size_t maxFiles = 5;
    
    // Serialization
    json toJson() const;
    static LoggingConfig fromJson(const json& j);
};

} // namespace OmniNet

#endif // OMNINET_CONFIG_H