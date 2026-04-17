// OmniNet Tools - Configuration Reader
#ifndef OMNINET_TOOLS_CONFIG_H
#define OMNINET_TOOLS_CONFIG_H

#include <string>
#include <optional>
#include <filesystem>
#include <variant>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace OmniNet::Tools {

// ============================================================================
// Config Value Types
// ============================================================================

using ConfigValue = std::variant<std::string, int64_t, double, bool>;

struct Config {
    // Version
    std::string version = "1.0";
    
    // Server defaults
    struct Server {
        std::string address = "0.0.0.0";
        uint16_t port = 8080;
        size_t workers = 0;
        int timeout = 30;
    } server;
    
    // Logging
    struct Logging {
        std::string level = "INFO";
        std::string file = "omninet.log";
        bool toConsole = true;
        bool toFile = true;
        size_t maxSizeMb = 10;
        size_t maxFiles = 5;
    } logging;
    
    // Database
    struct Database {
        std::string path = "omninet.db";
        int busyTimeoutMs = 5000;
        bool foreignKeys = true;
        bool walMode = true;
    } database;
    
    // CORS
    struct Cors {
        bool enabled = false;
        std::string origins = "*";
        std::string methods = "GET,POST,PUT,DELETE";
        std::string headers = "Content-Type,Authorization";
    } cors;
    
    // TLS
    struct Tls {
        bool enabled = false;
        std::string certFile;
        std::string keyFile;
    } tls;
    
    // Dashboard
    struct Dashboard {
        bool enabled = true;
        std::string urlPath = "/omninet";
        std::string apiPath = "/omninet/api";
        int sessionTtl = 3600;
        int maxLoginAttempts = 5;
    } dashboard;
    
    // Intrusion detection
    struct Intrusion {
        bool enabled = true;
        bool autoBlock = true;
        int blockDuration = 30;  // minutes
        size_t maxFailedAttempts = 10;
        int attemptWindow = 15;  // minutes
    } intrusion;
    
    // Raw JSON for any unknown fields
    json raw;
};

// ============================================================================
// ConfigReader
// ============================================================================

class ConfigReader {
public:
    ConfigReader();
    explicit ConfigReader(const std::filesystem::path& path);
    
    // Load config from file
    bool load(const std::filesystem::path& path);
    bool load();
    
    // Access config values
    const Config& config() const { return m_config; }
    Config& config() { return m_config; }
    
    // Get raw JSON value
    std::optional<json> get(const std::string& path);
    template<typename T>
    std::optional<T> getValue(const std::string& path);
    
    // Check if loaded
    bool isLoaded() const { return m_loaded; }
    
    // Get config file path
    const std::filesystem::path& path() const { return m_path; }
    
    // Validation
    bool isValid() const;
    std::vector<std::string> validationErrors() const;
    
    // Serialize to JSON
    json toJson() const;
    std::string toJsonString() const;

private:
    Config m_config;
    std::filesystem::path m_path;
    bool m_loaded = false;
    
    void loadFromJson(const json& j);
    void populateDefaults();
};

// ============================================================================
// Server Config (for omninet-ctl)
// ============================================================================

struct ServerInstanceConfig {
    std::string name;
    std::string address = "0.0.0.0";
    uint16_t port = 8080;
    std::string executable;
    std::string configFile;
    std::string logFile;
    bool tlsEnabled = false;
    std::string tlsCert;
    std::string tlsKey;
    
    // Serialization
    json toJson() const;
    static ServerInstanceConfig fromJson(const json& j);
    
    // PID file path for this server
    std::filesystem::path pidFile() const;
};

class ServerConfigReader {
public:
    ServerConfigReader();
    
    // List available server configs
    std::vector<ServerInstanceConfig> listServers();
    
    // Load specific server config
    std::optional<ServerInstanceConfig> loadServer(const std::string& name);
    
    // Save server config
    bool saveServer(const ServerInstanceConfig& config);
    
    // Delete server config
    bool deleteServer(const std::string& name);
    
    // Get servers directory
    std::filesystem::path serversDir() const;

private:
    std::filesystem::path m_serversDir;
};

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_CONFIG_H