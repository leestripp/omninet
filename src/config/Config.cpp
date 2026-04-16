// OmniNet - Configuration Implementation
#include "Config.h"
#include "utils/Logger.h"
#include <cstring>

namespace OmniNet::Config {

// ============================================================================
// Global Configuration
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
    
    // Load from environment
    loadFromEnvironment();
}

const std::filesystem::path& Config::dataDir() const {
    return m_dataDir;
}

void Config::setDataDir(const std::filesystem::path& path) {
    m_dataDir = path;
    std::filesystem::create_directories(m_dataDir);
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
    return m_logLevel;
}

void Config::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

const std::optional<std::string>& Config::logFile() const {
    return m_logFile;
}

void Config::setLogFile(const std::string& path) {
    m_logFile = path;
}

uint16_t Config::defaultPort() const {
    return m_defaultPort;
}

void Config::setDefaultPort(uint16_t port) {
    m_defaultPort = port;
}

std::chrono::seconds Config::defaultRequestTimeout() const {
    return m_defaultRequestTimeout;
}

void Config::setDefaultRequestTimeout(std::chrono::seconds timeout) {
    m_defaultRequestTimeout = timeout;
}

size_t Config::defaultWorkerCount() const {
    return m_defaultWorkerCount;
}

void Config::setDefaultWorkerCount(size_t count) {
    m_defaultWorkerCount = count;
}

const std::optional<std::string>& Config::defaultTlsCert() const {
    return m_defaultTlsCert;
}

void Config::setDefaultTlsCert(const std::string& path) {
    m_defaultTlsCert = path;
}

const std::optional<std::string>& Config::defaultTlsKey() const {
    return m_defaultTlsKey;
}

void Config::setDefaultTlsKey(const std::string& path) {
    m_defaultTlsKey = path;
}

const std::filesystem::path& Config::databasePath() const {
    return m_databasePath;
}

void Config::setDatabasePath(const std::filesystem::path& path) {
    m_databasePath = path;
}

void Config::loadFromEnvironment() {
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
    }
    
    if (const char* tlsCert = std::getenv("OMNINET_TLS_CERT")) {
        m_defaultTlsCert = tlsCert;
    }
    
    if (const char* tlsKey = std::getenv("OMNINET_TLS_KEY")) {
        m_defaultTlsKey = tlsKey;
    }
}

void Config::loadFromFile(const std::string& path) {
    // TODO: Implement file loading (JSON or env format)
    (void)path;
}

} // namespace OmniNet::Config
