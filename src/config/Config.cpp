// OmniNet - Configuration Implementation
#include "Config.h"
#include "utils/Logger.h"

#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace std::chrono_literals;

namespace OmniNet {

// ============================================================================
// Config Implementation
// ============================================================================

Config& Config::instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    // Set default data directory (~/.omninet)
    const char* home = std::getenv("HOME");
    if (home) {
        m_dataDir = std::filesystem::path(home) / ".omninet";
    } else {
        m_dataDir = "/tmp/.omninet";
    }
    
    // Ensure directory exists
    std::filesystem::create_directories(m_dataDir);
    
    // Default database path
    m_databasePath = m_dataDir / "omninet.db";
    
    // Default log file
    m_logFile = (m_dataDir / "omninet.log").string();
    
    // Load from environment first
    loadFromEnvironment();
    
    // Then try to load from config file
    auto configPath = configFile();
    if (std::filesystem::exists(configPath)) {
        loadFromFile(configPath.string());
    } else {
        // Create default config file
        createDefaultConfig();
    }
}

Config::~Config() {
    stopFileWatcher();
}

std::filesystem::path Config::configFile() const {
    return m_dataDir / "config.json";
}

const std::filesystem::path& Config::dataDir() const {
    return m_dataDir;
}

void Config::setDataDir(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataDir = path;
    std::filesystem::create_directories(m_dataDir);
    m_databasePath = m_dataDir / "omninet.db";
    if (!m_logFile) {
        m_logFile = (m_dataDir / "omninet.log").string();
    }
}

std::filesystem::path Config::resolve(const std::string& filename) const {
    if (filename.empty()) {
        return m_dataDir;
    }
    if (filename[0] == '/') {
        return filename;
    }
    return m_dataDir / filename;
}

LogLevel Config::logLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_logLevel;
}

void Config::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logLevel = level;
}

const std::optional<std::string>& Config::logFile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_logFile;
}

void Config::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFile = path;
}

uint16_t Config::defaultPort() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultPort;
}

void Config::setDefaultPort(uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultPort = port;
}

std::chrono::seconds Config::defaultRequestTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultRequestTimeout;
}

void Config::setDefaultRequestTimeout(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultRequestTimeout = timeout;
}

size_t Config::defaultWorkerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultWorkerCount;
}

void Config::setDefaultWorkerCount(size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultWorkerCount = count;
}

const std::optional<std::string>& Config::defaultTlsCert() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultTlsCert;
}

void Config::setDefaultTlsCert(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultTlsCert = path;
}

const std::optional<std::string>& Config::defaultTlsKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultTlsKey;
}

void Config::setDefaultTlsKey(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultTlsKey = path;
}

const std::filesystem::path& Config::databasePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_databasePath;
}

void Config::setDatabasePath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_databasePath = path;
}

void Config::loadFromEnvironment() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Load from environment variables
    if (const char* port = std::getenv("OMNINET_PORT")) {
        m_defaultPort = static_cast<uint16_t>(std::atoi(port));
    }
    
    if (const char* workers = std::getenv("OMNINET_WORKERS")) {
        m_defaultWorkerCount = static_cast<size_t>(std::atoi(workers));
    }
    
    if (const char* timeout = std::getenv("OMNINET_TIMEOUT")) {
        m_defaultRequestTimeout = std::chrono::seconds(std::atoi(timeout));
    }
    
    if (const char* logLevel = std::getenv("OMNINET_LOG_LEVEL")) {
        if (strcmp(logLevel, "DEBUG") == 0) m_logLevel = LogLevel::DEBUG;
        else if (strcmp(logLevel, "INFO") == 0) m_logLevel = LogLevel::INFO;
        else if (strcmp(logLevel, "WARN") == 0) m_logLevel = LogLevel::WARN;
        else if (strcmp(logLevel, "ERROR") == 0) m_logLevel = LogLevel::ERROR;
        else if (strcmp(logLevel, "CRITICAL") == 0) m_logLevel = LogLevel::CRITICAL;
    }
    
    if (const char* tlsCert = std::getenv("OMNINET_TLS_CERT")) {
        m_defaultTlsCert = tlsCert;
    }
    
    if (const char* tlsKey = std::getenv("OMNINET_TLS_KEY")) {
        m_defaultTlsKey = tlsKey;
    }
    
    if (const char* dbPath = std::getenv("OMNINET_DATABASE")) {
        m_databasePath = dbPath;
    }
    
    if (const char* dataDir = std::getenv("OMNINET_DATA_DIR")) {
        m_dataDir = dataDir;
        std::filesystem::create_directories(m_dataDir);
    }
}

void Config::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            Logger::instance().error("Config: Failed to open file: " + path);
            return;
        }
        
        json j;
        file >> j;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        loadFromJson(j);
        
        // Update last modified time
        m_configLastModified = std::filesystem::last_write_time(path);
        
    } catch (const json::parse_error& e) {
        Logger::instance().error("Config: JSON parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        Logger::instance().error("Config: Failed to load: " + std::string(e.what()));
    }
}

void Config::load() {
    loadFromFile(configFile().string());
}

void Config::loadFromJson(const json& j) {
    // Version
    if (j.contains("version")) {
        // Could add version migration logic here
    }
    
    // Server section
    if (j.contains("server")) {
        const auto& server = j["server"];
        if (server.contains("port")) m_defaultPort = server["port"].get<uint16_t>();
        if (server.contains("workers")) m_defaultWorkerCount = server["workers"].get<size_t>();
        if (server.contains("requestTimeout")) {
            m_defaultRequestTimeout = std::chrono::seconds(server["requestTimeout"].get<int>());
        }
        if (server.contains("address")) {
            // Would be used when creating a server instance
        }
    }
    
    // TLS section
    if (j.contains("tls")) {
        const auto& tls = j["tls"];
        if (tls.contains("certFile") && !tls["certFile"].empty()) {
            m_defaultTlsCert = tls["certFile"].get<std::string>();
        }
        if (tls.contains("keyFile") && !tls["keyFile"].empty()) {
            m_defaultTlsKey = tls["keyFile"].get<std::string>();
        }
    }
    
    // Database section
    if (j.contains("database")) {
        const auto& db = j["database"];
        if (db.contains("path") && !db["path"].empty()) {
            m_databasePath = db["path"].get<std::string>();
        }
    }
    
    // Logging section
    if (j.contains("logging")) {
        const auto& logging = j["logging"];
        if (logging.contains("level")) {
            std::string level = logging["level"].get<std::string>();
            if (level == "DEBUG") m_logLevel = LogLevel::DEBUG;
            else if (level == "INFO") m_logLevel = LogLevel::INFO;
            else if (level == "WARN") m_logLevel = LogLevel::WARN;
            else if (level == "ERROR") m_logLevel = LogLevel::ERROR;
            else if (level == "CRITICAL") m_logLevel = LogLevel::CRITICAL;
        }
        if (logging.contains("file") && !logging["file"].empty()) {
            m_logFile = logging["file"].get<std::string>();
        }
    }
}

bool Config::validate() const {
    m_validationErrors.clear();
    
    // Port validation
    if (m_defaultPort == 0) {
        m_validationErrors.push_back("Port must be between 1 and 65535");
    }
    
    // Timeout validation
    if (m_defaultRequestTimeout.count() < 1 || m_defaultRequestTimeout.count() > 3600) {
        m_validationErrors.push_back("Request timeout must be between 1 and 3600 seconds");
    }
    
    // Worker count validation
    if (m_defaultWorkerCount > 256) {
        m_validationErrors.push_back("Worker count must be between 0 (auto) and 256");
    }
    
    // TLS files validation
    if (m_defaultTlsCert && !m_defaultTlsCert->empty()) {
        if (!std::filesystem::exists(*m_defaultTlsCert)) {
            m_validationErrors.push_back("TLS certificate file does not exist: " + *m_defaultTlsCert);
        }
    }
    if (m_defaultTlsKey && !m_defaultTlsKey->empty()) {
        if (!std::filesystem::exists(*m_defaultTlsKey)) {
            m_validationErrors.push_back("TLS key file does not exist: " + *m_defaultTlsKey);
        }
    }
    
    // Data directory validation
    if (!std::filesystem::exists(m_dataDir)) {
        // Try to create
        try {
            std::filesystem::create_directories(m_dataDir);
        } catch (const std::exception& e) {
            m_validationErrors.push_back("Cannot create data directory: " + m_dataDir.string());
        }
    }
    
    return m_validationErrors.empty();
}

std::vector<std::string> Config::validationErrors() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_validationErrors;
}

bool Config::reload() {
    auto path = configFile().string();
    Logger::instance().info("Config: Reloading from " + path);
    loadFromFile(path);
    
    // Notify callbacks
    for (const auto& cb : m_changeCallbacks) {
        cb("config");
    }
    
    return validate();
}

void Config::startFileWatcher() {
    if (m_watcherRunning.load()) return;
    
    m_watcherRunning = true;
    m_watcherThread = std::thread(&Config::fileWatcherLoop, this);
}

void Config::stopFileWatcher() {
    m_watcherRunning = false;
    if (m_watcherThread.joinable()) {
        m_watcherThread.join();
    }
}

void Config::fileWatcherLoop() {
    auto configPath = configFile();
    
    // Get initial modification time
    try {
        m_configLastModified = std::filesystem::last_write_time(configPath);
    } catch (...) {
        m_configLastModified = std::filesystem::file_time_type::min();
    }
    
    while (m_watcherRunning.load()) {
        try {
            auto currentModified = std::filesystem::last_write_time(configPath);
            if (currentModified > m_configLastModified) {
            std::string msg = "Config: File change detected, reloading...";
            Logger::instance().info(msg);
                m_configLastModified = currentModified;
                reload();
            }
        } catch (...) {
            // Ignore errors during polling
        }
        
        std::this_thread::sleep_for(1s);
    }
}

void Config::onChange(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(callback);
}

std::string Config::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return toJsonInternal().dump(2);
}

json Config::toJsonInternal() const {
    json j;
    
    j["version"] = "1.0";
    
    // Server
    j["server"] = {
        {"address", "0.0.0.0"},
        {"port", m_defaultPort},
        {"workers", m_defaultWorkerCount},
        {"requestTimeout", static_cast<int>(m_defaultRequestTimeout.count())},
        {"keepAliveTimeout", 5},
        {"maxRequestSize", 1048576}
    };
    
    // TLS
    j["tls"] = {
        {"enabled", m_defaultTlsCert.has_value() && !m_defaultTlsCert->empty()},
        {"certFile", m_defaultTlsCert.value_or("")},
        {"keyFile", m_defaultTlsKey.value_or("")},
        {"verifyClient", false}
    };
    
    // CORS (default)
    j["cors"] = {
        {"enabled", false},
        {"allowedOrigins", "*"},
        {"allowedMethods", "GET,POST,PUT,DELETE"},
        {"allowedHeaders", "Content-Type,Authorization"},
        {"maxAge", 86400}
    };
    
    // WebSocket (default)
    j["websocket"] = {
        {"pingInterval", 30},
        {"pongTimeout", 10},
        {"maxMessageSize", 1048576},
        {"maxConnections", 1024}
    };
    
    // Database
    j["database"] = {
        {"path", m_databasePath.string()},
        {"busyTimeoutMs", 5000},
        {"foreignKeys", true},
        {"walMode", true}
    };
    
    // Dashboard (default)
    j["dashboard"] = {
        {"enabled", true},
        {"urlPath", "/omninet"},
        {"apiPath", "/omninet/api"},
        {"sessionTtl", 3600},
        {"maxLoginAttempts", 5},
        {"lockoutDuration", 300}
    };
    
    // Intrusion (default)
    j["intrusion"] = {
        {"enabled", true},
        {"autoBlock", true},
        {"blockDuration", 30},
        {"maxFailedAttempts", 10},
        {"attemptWindow", 15}
    };
    
    // Logging
    std::string levelStr = "INFO";
    switch (m_logLevel) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARN: levelStr = "WARN"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        case LogLevel::CRITICAL: levelStr = "CRITICAL"; break;
    }
    j["logging"] = {
        {"level", levelStr},
        {"file", m_logFile.value_or("omninet.log")},
        {"toConsole", true},
        {"toFile", true},
        {"maxSizeMb", 10},
        {"maxFiles", 5}
    };
    
    return j;
}

bool Config::save(const std::string& path) {
    auto filePath = path.empty() ? configFile().string() : path;
    
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream file(filePath);
        if (!file.is_open()) {
            Logger::instance().error("Config: Failed to open for writing: " + filePath);
            return false;
        }
        
        file << toJsonInternal().dump(2);
        Logger::instance().info("Config: Saved to " + filePath);
        return true;
        
    } catch (const std::exception& e) {
        Logger::instance().error("Config: Failed to save: " + std::string(e.what()));
        return false;
    }
}

void Config::createDefaultConfig() {
    auto path = configFile();
    
    if (std::filesystem::exists(path)) {
        return; // Don't overwrite existing config
    }
    
    Logger::instance().info("Config: Creating default config at " + path.string());
    
    json defaultConfig = {
        {"version", "1.0"},
        {"server", {
            {"address", "0.0.0.0"},
            {"port", 8080},
            {"workers", 0},
            {"requestTimeout", 30},
            {"keepAliveTimeout", 5},
            {"maxRequestSize", 1048576}
        }},
        {"tls", {
            {"enabled", false},
            {"certFile", ""},
            {"keyFile", ""},
            {"verifyClient", false}
        }},
        {"cors", {
            {"enabled", false},
            {"allowedOrigins", "*"},
            {"allowedMethods", "GET,POST,PUT,DELETE"},
            {"allowedHeaders", "Content-Type,Authorization"},
            {"maxAge", 86400}
        }},
        {"websocket", {
            {"pingInterval", 30},
            {"pongTimeout", 10},
            {"maxMessageSize", 1048576},
            {"maxFrameSize", 65536},
            {"maxConnections", 1024},
            {"autoFragment", true},
            {"fragmentSize", 4096}
        }},
        {"database", {
            {"path", "omninet.db"},
            {"busyTimeoutMs", 5000},
            {"foreignKeys", true},
            {"walMode", true},
            {"cacheSizeKb", 2000}
        }},
        {"dashboard", {
            {"enabled", true},
            {"urlPath", "/omninet"},
            {"apiPath", "/omninet/api"},
            {"allowRemoteAccess", false},
            {"allowedIpPrefixes", {"127.", "::1"}},
            {"sessionTtl", 3600},
            {"maxLoginAttempts", 5},
            {"lockoutDuration", 300}
        }},
        {"intrusion", {
            {"enabled", true},
            {"autoBlock", true},
            {"blockDuration", 30},
            {"maxFailedAttempts", 10},
            {"attemptWindow", 15},
            {"maxLogEntries", 10000}
        }},
        {"logging", {
            {"level", "INFO"},
            {"file", "omninet.log"},
            {"toConsole", true},
            {"toFile", true},
            {"maxSizeMb", 10},
            {"maxFiles", 5}
        }}
    };
    
    try {
        std::ofstream file(path);
        if (file.is_open()) {
            file << defaultConfig.dump(2);
            file.close();
            m_configLastModified = std::filesystem::last_write_time(path);
        }
    } catch (const std::exception& e) {
        Logger::instance().error("Config: Failed to create default config: " + std::string(e.what()));
    }
}

// ============================================================================
// ServerConfig Serialization
// ============================================================================

json ServerConfig::toJson() const {
    return {
        {"address", address},
        {"port", port},
        {"workers", workerCount},
        {"requestTimeout", static_cast<int>(requestTimeout.count())},
        {"keepAliveTimeout", static_cast<int>(keepAliveTimeout.count())},
        {"maxRequestSize", maxRequestSize},
        {"tls", {
            {"enabled", tlsEnabled},
            {"certFile", tlsCertFile},
            {"keyFile", tlsKeyFile},
            {"verifyClient", tlsVerifyClient}
        }},
        {"cors", {
            {"enabled", corsEnabled},
            {"allowedOrigins", corsAllowedOrigins},
            {"allowedMethods", corsAllowedMethods},
            {"allowedHeaders", corsAllowedHeaders},
            {"maxAge", static_cast<int>(corsMaxAge.count())}
        }}
    };
}

ServerConfig ServerConfig::fromJson(const json& j) {
    ServerConfig cfg;
    
    if (j.contains("address")) cfg.address = j["address"].get<std::string>();
    if (j.contains("port")) cfg.port = j["port"].get<uint16_t>();
    if (j.contains("workers")) cfg.workerCount = j["workers"].get<size_t>();
    if (j.contains("requestTimeout")) cfg.requestTimeout = std::chrono::seconds(j["requestTimeout"].get<int>());
    if (j.contains("keepAliveTimeout")) cfg.keepAliveTimeout = std::chrono::seconds(j["keepAliveTimeout"].get<int>());
    if (j.contains("maxRequestSize")) cfg.maxRequestSize = j["maxRequestSize"].get<size_t>();
    
    if (j.contains("tls")) {
        const auto& tls = j["tls"];
        if (tls.contains("enabled")) cfg.tlsEnabled = tls["enabled"].get<bool>();
        if (tls.contains("certFile")) cfg.tlsCertFile = tls["certFile"].get<std::string>();
        if (tls.contains("keyFile")) cfg.tlsKeyFile = tls["keyFile"].get<std::string>();
        if (tls.contains("verifyClient")) cfg.tlsVerifyClient = tls["verifyClient"].get<bool>();
    }
    
    if (j.contains("cors")) {
        const auto& cors = j["cors"];
        if (cors.contains("enabled")) cfg.corsEnabled = cors["enabled"].get<bool>();
        if (cors.contains("allowedOrigins")) cfg.corsAllowedOrigins = cors["allowedOrigins"].get<std::string>();
        if (cors.contains("allowedMethods")) cfg.corsAllowedMethods = cors["allowedMethods"].get<std::string>();
        if (cors.contains("allowedHeaders")) cfg.corsAllowedHeaders = cors["allowedHeaders"].get<std::string>();
        if (cors.contains("maxAge")) cfg.corsMaxAge = std::chrono::seconds(cors["maxAge"].get<int>());
    }
    
    return cfg;
}

// ============================================================================
// WebSocketConfig Serialization
// ============================================================================

json WebSocketConfig::toJson() const {
    return {
        {"pingInterval", static_cast<int>(pingInterval.count())},
        {"pongTimeout", static_cast<int>(pongTimeout.count())},
        {"maxMessageSize", maxMessageSize},
        {"maxFrameSize", maxFrameSize},
        {"maxConnections", maxConnections},
        {"autoFragment", autoFragment},
        {"fragmentSize", fragmentSize}
    };
}

WebSocketConfig WebSocketConfig::fromJson(const json& j) {
    WebSocketConfig cfg;
    
    if (j.contains("pingInterval")) cfg.pingInterval = std::chrono::seconds(j["pingInterval"].get<int>());
    if (j.contains("pongTimeout")) cfg.pongTimeout = std::chrono::seconds(j["pongTimeout"].get<int>());
    if (j.contains("maxMessageSize")) cfg.maxMessageSize = j["maxMessageSize"].get<size_t>();
    if (j.contains("maxFrameSize")) cfg.maxFrameSize = j["maxFrameSize"].get<size_t>();
    if (j.contains("maxConnections")) cfg.maxConnections = j["maxConnections"].get<size_t>();
    if (j.contains("autoFragment")) cfg.autoFragment = j["autoFragment"].get<bool>();
    if (j.contains("fragmentSize")) cfg.fragmentSize = j["fragmentSize"].get<size_t>();
    
    return cfg;
}

// ============================================================================
// DatabaseConfig Serialization
// ============================================================================

json DatabaseConfig::toJson() const {
    return {
        {"path", path},
        {"busyTimeoutMs", busyTimeoutMs},
        {"foreignKeys", foreignKeysEnabled},
        {"walMode", WALModeEnabled},
        {"cacheSizeKb", cacheSizeKb}
    };
}

DatabaseConfig DatabaseConfig::fromJson(const json& j) {
    DatabaseConfig cfg;
    
    if (j.contains("path")) cfg.path = j["path"].get<std::string>();
    if (j.contains("busyTimeoutMs")) cfg.busyTimeoutMs = j["busyTimeoutMs"].get<int>();
    if (j.contains("foreignKeys")) cfg.foreignKeysEnabled = j["foreignKeys"].get<bool>();
    if (j.contains("walMode")) cfg.WALModeEnabled = j["walMode"].get<bool>();
    if (j.contains("cacheSizeKb")) cfg.cacheSizeKb = j["cacheSizeKb"].get<size_t>();
    
    return cfg;
}

// ============================================================================
// DashboardConfig Serialization
// ============================================================================

json DashboardConfig::toJson() const {
    return {
        {"urlPath", urlPath},
        {"apiPath", apiPath},
        {"enabled", enabled},
        {"allowRemoteAccess", allowRemoteAccess},
        {"allowedIpPrefixes", allowedIpPrefixes},
        {"sessionTtl", sessionTtlSeconds},
        {"maxLoginAttempts", maxLoginAttempts},
        {"lockoutDuration", static_cast<int>(lockoutDuration.count())}
    };
}

DashboardConfig DashboardConfig::fromJson(const json& j) {
    DashboardConfig cfg;
    
    if (j.contains("urlPath")) cfg.urlPath = j["urlPath"].get<std::string>();
    if (j.contains("apiPath")) cfg.apiPath = j["apiPath"].get<std::string>();
    if (j.contains("enabled")) cfg.enabled = j["enabled"].get<bool>();
    if (j.contains("allowRemoteAccess")) cfg.allowRemoteAccess = j["allowRemoteAccess"].get<bool>();
    if (j.contains("allowedIpPrefixes")) cfg.allowedIpPrefixes = j["allowedIpPrefixes"].get<std::vector<std::string>>();
    if (j.contains("sessionTtl")) cfg.sessionTtlSeconds = j["sessionTtl"].get<int>();
    if (j.contains("maxLoginAttempts")) cfg.maxLoginAttempts = j["maxLoginAttempts"].get<int>();
    if (j.contains("lockoutDuration")) cfg.lockoutDuration = std::chrono::seconds(j["lockoutDuration"].get<int>());
    
    return cfg;
}

// ============================================================================
// IntrusionConfig Serialization
// ============================================================================

json IntrusionConfig::toJson() const {
    return {
        {"enabled", enabled},
        {"autoBlock", autoBlock},
        {"blockDuration", static_cast<int>(blockDuration.count())},
        {"maxFailedAttempts", maxFailedAttempts},
        {"attemptWindow", static_cast<int>(attemptWindow.count())},
        {"maxLogEntries", maxLogEntries}
    };
}

IntrusionConfig IntrusionConfig::fromJson(const json& j) {
    IntrusionConfig cfg;
    
    if (j.contains("enabled")) cfg.enabled = j["enabled"].get<bool>();
    if (j.contains("autoBlock")) cfg.autoBlock = j["autoBlock"].get<bool>();
    if (j.contains("blockDuration")) cfg.blockDuration = std::chrono::minutes(j["blockDuration"].get<int>());
    if (j.contains("maxFailedAttempts")) cfg.maxFailedAttempts = j["maxFailedAttempts"].get<size_t>();
    if (j.contains("attemptWindow")) cfg.attemptWindow = std::chrono::minutes(j["attemptWindow"].get<int>());
    if (j.contains("maxLogEntries")) cfg.maxLogEntries = j["maxLogEntries"].get<size_t>();
    
    return cfg;
}

// ============================================================================
// LoggingConfig Serialization
// ============================================================================

json LoggingConfig::toJson() const {
    return {
        {"level", level},
        {"file", file},
        {"toConsole", toConsole},
        {"toFile", toFile},
        {"maxSizeMb", maxSizeMb},
        {"maxFiles", maxFiles}
    };
}

LoggingConfig LoggingConfig::fromJson(const json& j) {
    LoggingConfig cfg;
    
    if (j.contains("level")) cfg.level = j["level"].get<std::string>();
    if (j.contains("file")) cfg.file = j["file"].get<std::string>();
    if (j.contains("toConsole")) cfg.toConsole = j["toConsole"].get<bool>();
    if (j.contains("toFile")) cfg.toFile = j["toFile"].get<bool>();
    if (j.contains("maxSizeMb")) cfg.maxSizeMb = j["maxSizeMb"].get<size_t>();
    if (j.contains("maxFiles")) cfg.maxFiles = j["maxFiles"].get<size_t>();
    
    return cfg;
}

} // namespace OmniNet