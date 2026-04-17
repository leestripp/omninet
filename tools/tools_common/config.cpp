// OmniNet Tools - Configuration Reader Implementation
#include "config.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include "common.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace OmniNet::Tools {

// ============================================================================
// ConfigReader Implementation
// ============================================================================

ConfigReader::ConfigReader() 
    : m_path(Paths::configFile()) {
    load();
}

ConfigReader::ConfigReader(const std::filesystem::path& path) 
    : m_path(path) {
    load(path);
}

bool ConfigReader::load(const std::filesystem::path& path) {
    m_path = path;
    return load();
}

bool ConfigReader::load() {
    if (!fs::exists(m_path)) {
        m_loaded = false;
        populateDefaults();
        return false;
    }
    
    try {
        std::ifstream file(m_path);
        if (!file.is_open()) {
            m_loaded = false;
            return false;
        }
        
        json j;
        file >> j;
        loadFromJson(j);
        m_loaded = true;
        return true;
        
    } catch (const json::parse_error& e) {
        std::cerr << "Failed to parse config: " << e.what() << '\n';
        m_loaded = false;
        populateDefaults();
        return false;
    }
}

void ConfigReader::loadFromJson(const json& j) {
    populateDefaults();
    m_config.raw = j;
    
    // Version
    if (j.contains("version")) {
        m_config.version = j["version"].get<std::string>();
    }
    
    // Server
    if (j.contains("server")) {
        const auto& s = j["server"];
        if (s.contains("address")) m_config.server.address = s["address"].get<std::string>();
        if (s.contains("port")) m_config.server.port = s["port"].get<uint16_t>();
        if (s.contains("workers")) m_config.server.workers = s["workers"].get<size_t>();
        if (s.contains("timeout")) m_config.server.timeout = s["timeout"].get<int>();
    }
    
    // Logging
    if (j.contains("logging")) {
        const auto& l = j["logging"];
        if (l.contains("level")) m_config.logging.level = l["level"].get<std::string>();
        if (l.contains("file")) m_config.logging.file = l["file"].get<std::string>();
        if (l.contains("toConsole")) m_config.logging.toConsole = l["toConsole"].get<bool>();
        if (l.contains("toFile")) m_config.logging.toFile = l["toFile"].get<bool>();
        if (l.contains("maxSizeMb")) m_config.logging.maxSizeMb = l["maxSizeMb"].get<size_t>();
        if (l.contains("maxFiles")) m_config.logging.maxFiles = l["maxFiles"].get<size_t>();
    }
    
    // Database
    if (j.contains("database")) {
        const auto& d = j["database"];
        if (d.contains("path")) m_config.database.path = d["path"].get<std::string>();
        if (d.contains("busyTimeoutMs")) m_config.database.busyTimeoutMs = d["busyTimeoutMs"].get<int>();
        if (d.contains("foreignKeys")) m_config.database.foreignKeys = d["foreignKeys"].get<bool>();
        if (d.contains("walMode")) m_config.database.walMode = d["walMode"].get<bool>();
    }
    
    // CORS
    if (j.contains("cors")) {
        const auto& c = j["cors"];
        if (c.contains("enabled")) m_config.cors.enabled = c["enabled"].get<bool>();
        if (c.contains("origins")) m_config.cors.origins = c["origins"].get<std::string>();
        if (c.contains("methods")) m_config.cors.methods = c["methods"].get<std::string>();
        if (c.contains("headers")) m_config.cors.headers = c["headers"].get<std::string>();
    }
    
    // TLS
    if (j.contains("tls")) {
        const auto& t = j["tls"];
        if (t.contains("enabled")) m_config.tls.enabled = t["enabled"].get<bool>();
        if (t.contains("certFile")) m_config.tls.certFile = t["certFile"].get<std::string>();
        if (t.contains("keyFile")) m_config.tls.keyFile = t["keyFile"].get<std::string>();
    }
    
    // Dashboard
    if (j.contains("dashboard")) {
        const auto& d = j["dashboard"];
        if (d.contains("enabled")) m_config.dashboard.enabled = d["enabled"].get<bool>();
        if (d.contains("urlPath")) m_config.dashboard.urlPath = d["urlPath"].get<std::string>();
        if (d.contains("apiPath")) m_config.dashboard.apiPath = d["apiPath"].get<std::string>();
        if (d.contains("sessionTtl")) m_config.dashboard.sessionTtl = d["sessionTtl"].get<int>();
        if (d.contains("maxLoginAttempts")) m_config.dashboard.maxLoginAttempts = d["maxLoginAttempts"].get<int>();
    }
    
    // Intrusion
    if (j.contains("intrusion")) {
        const auto& i = j["intrusion"];
        if (i.contains("enabled")) m_config.intrusion.enabled = i["enabled"].get<bool>();
        if (i.contains("autoBlock")) m_config.intrusion.autoBlock = i["autoBlock"].get<bool>();
        if (i.contains("blockDuration")) m_config.intrusion.blockDuration = i["blockDuration"].get<int>();
        if (i.contains("maxFailedAttempts")) m_config.intrusion.maxFailedAttempts = i["maxFailedAttempts"].get<size_t>();
        if (i.contains("attemptWindow")) m_config.intrusion.attemptWindow = i["attemptWindow"].get<int>();
    }
}

void ConfigReader::populateDefaults() {
    // Defaults are set in the Config struct initialization
}

std::optional<json> ConfigReader::get(const std::string& path) {
    // Simple dot-notation path getter
    // e.g., "server.port" -> j["server"]["port"]
    if (!m_loaded) return std::nullopt;
    
    auto parts = split(path, '.');
    json* current = &m_config.raw;
    
    for (const auto& part : parts) {
        if (!current->contains(part)) {
            return std::nullopt;
        }
        current = &(*current)[part];
    }
    
    return *current;
}

template<typename T>
std::optional<T> ConfigReader::getValue(const std::string& path) {
    auto j = get(path);
    if (!j) return std::nullopt;
    try {
        return j->get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

bool ConfigReader::isValid() const {
    return validationErrors().empty();
}

std::vector<std::string> ConfigReader::validationErrors() const {
    std::vector<std::string> errors;
    
    if (m_config.server.port == 0) {
        errors.push_back("Server port cannot be 0");
    }
    
    if (m_config.server.port > 65535) {
        errors.push_back("Server port must be <= 65535");
    }
    
    return errors;
}

json ConfigReader::toJson() const {
    return {
        {"version", m_config.version},
        {"server", {
            {"address", m_config.server.address},
            {"port", m_config.server.port},
            {"workers", m_config.server.workers},
            {"timeout", m_config.server.timeout}
        }},
        {"logging", {
            {"level", m_config.logging.level},
            {"file", m_config.logging.file},
            {"toConsole", m_config.logging.toConsole},
            {"toFile", m_config.logging.toFile},
            {"maxSizeMb", m_config.logging.maxSizeMb},
            {"maxFiles", m_config.logging.maxFiles}
        }},
        {"database", {
            {"path", m_config.database.path},
            {"busyTimeoutMs", m_config.database.busyTimeoutMs},
            {"foreignKeys", m_config.database.foreignKeys},
            {"walMode", m_config.database.walMode}
        }},
        {"cors", {
            {"enabled", m_config.cors.enabled},
            {"origins", m_config.cors.origins},
            {"methods", m_config.cors.methods},
            {"headers", m_config.cors.headers}
        }},
        {"tls", {
            {"enabled", m_config.tls.enabled},
            {"certFile", m_config.tls.certFile},
            {"keyFile", m_config.tls.keyFile}
        }},
        {"dashboard", {
            {"enabled", m_config.dashboard.enabled},
            {"urlPath", m_config.dashboard.urlPath},
            {"apiPath", m_config.dashboard.apiPath},
            {"sessionTtl", m_config.dashboard.sessionTtl},
            {"maxLoginAttempts", m_config.dashboard.maxLoginAttempts}
        }},
        {"intrusion", {
            {"enabled", m_config.intrusion.enabled},
            {"autoBlock", m_config.intrusion.autoBlock},
            {"blockDuration", m_config.intrusion.blockDuration},
            {"maxFailedAttempts", m_config.intrusion.maxFailedAttempts},
            {"attemptWindow", m_config.intrusion.attemptWindow}
        }}
    };
}

std::string ConfigReader::toJsonString() const {
    return toJson().dump(2);
}

// ============================================================================
// ServerInstanceConfig Implementation
// ============================================================================

json ServerInstanceConfig::toJson() const {
    json j = {
        {"name", name},
        {"address", address},
        {"port", port}
    };
    if (!executable.empty()) j["executable"] = executable;
    if (!configFile.empty()) j["configFile"] = configFile;
    if (!logFile.empty()) j["logFile"] = logFile;
    j["tlsEnabled"] = tlsEnabled;
    if (tlsEnabled) {
        j["tlsCert"] = tlsCert;
        j["tlsKey"] = tlsKey;
    }
    return j;
}

ServerInstanceConfig ServerInstanceConfig::fromJson(const json& j) {
    ServerInstanceConfig cfg;
    cfg.name = j.value("name", "unnamed");
    cfg.address = j.value("address", "0.0.0.0");
    cfg.port = j.value("port", 8080);
    cfg.executable = j.value("executable", "");
    cfg.configFile = j.value("configFile", "");
    cfg.logFile = j.value("logFile", "");
    cfg.tlsEnabled = j.value("tlsEnabled", false);
    cfg.tlsCert = j.value("tlsCert", "");
    cfg.tlsKey = j.value("tlsKey", "");
    return cfg;
}

std::filesystem::path ServerInstanceConfig::pidFile() const {
    auto safeName = name;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    safeName = "omninet-" + safeName + ".pid";
    return Paths::dataDir() / safeName;
}

// ============================================================================
// ServerConfigReader Implementation
// ============================================================================

ServerConfigReader::ServerConfigReader() 
    : m_serversDir(Paths::serversDir()) {
    fs::create_directories(m_serversDir);
}

std::vector<ServerInstanceConfig> ServerConfigReader::listServers() {
    std::vector<ServerInstanceConfig> servers;
    
    if (!fs::exists(m_serversDir)) {
        return servers;
    }
    
    for (const auto& entry : fs::directory_iterator(m_serversDir)) {
        if (entry.path().extension() == ".json") {
            try {
                std::ifstream file(entry.path());
                json j;
                file >> j;
                servers.push_back(ServerInstanceConfig::fromJson(j));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to load server config " 
                          << entry.path() << ": " << e.what() << '\n';
            }
        }
    }
    
    return servers;
}

std::optional<ServerInstanceConfig> ServerConfigReader::loadServer(const std::string& name) {
    auto safeName = name;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    auto path = m_serversDir / (safeName + ".json");
    
    if (!fs::exists(path)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(path);
        json j;
        file >> j;
        return ServerInstanceConfig::fromJson(j);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to load server config: " << e.what() << '\n';
        return std::nullopt;
    }
}

bool ServerConfigReader::saveServer(const ServerInstanceConfig& config) {
    auto safeName = config.name;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    auto path = m_serversDir / (safeName + ".json");
    
    try {
        std::ofstream file(path);
        file << config.toJson().dump(2);
        return !file.fail();
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to save server config: " << e.what() << '\n';
        return false;
    }
}

bool ServerConfigReader::deleteServer(const std::string& name) {
    auto safeName = name;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    auto path = m_serversDir / (safeName + ".json");
    
    if (!fs::exists(path)) {
        return false;
    }
    
    return fs::remove(path);
}

fs::path ServerConfigReader::serversDir() const {
    return m_serversDir;
}

} // namespace OmniNet::Tools
