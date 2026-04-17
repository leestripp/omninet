// OmniNet Tools - Common Utilities Implementation
#include "common.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <iostream>

namespace fs = std::filesystem;

namespace OmniNet::Tools {

// ============================================================================
// Paths Implementation
// ============================================================================

std::filesystem::path Paths::dataDir() {
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".omninet";
    }
    // Fallback
    return fs::path("/tmp/.omninet");
}

std::filesystem::path Paths::configFile() {
    return dataDir() / "config.json";
}

std::filesystem::path Paths::databaseFile() {
    return dataDir() / "omninet.db";
}

std::filesystem::path Paths::logFile() {
    return dataDir() / "omninet.log";
}

std::filesystem::path Paths::certsDir() {
    return dataDir() / "certs";
}

std::filesystem::path Paths::serversDir() {
    return dataDir() / "servers";
}

std::filesystem::path Paths::resolve(const std::string& filename) {
    if (filename.empty()) {
        return dataDir();
    }
    if (filename[0] == '/') {
        return filename;
    }
    return dataDir() / filename;
}

std::string Paths::expandHome(const std::string& path) {
    if (path.starts_with("~/")) {
        const char* home = std::getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

// ============================================================================
// Time Helpers
// ============================================================================

std::string formatTimestamp(time_t timestamp) {
    std::tm tm = *std::localtime(&timestamp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::string formatDuration(std::chrono::seconds duration) {
    auto secs = duration.count();
    if (secs < 60) {
        return std::to_string(secs) + "s";
    }
    auto mins = secs / 60;
    secs %= 60;
    if (mins < 60) {
        return std::to_string(mins) + "m " + std::to_string(secs) + "s";
    }
    auto hours = mins / 60;
    mins %= 60;
    return std::to_string(hours) + "h " + std::to_string(mins) + "m";
}

std::string timeAgo(std::chrono::system_clock::time_point time) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - time);
    auto secs = duration.count();
    
    if (secs < 60) {
        return std::to_string(secs) + "s ago";
    }
    auto mins = secs / 60;
    if (mins < 60) {
        return std::to_string(mins) + "m ago";
    }
    auto hours = mins / 60;
    if (hours < 24) {
        return std::to_string(hours) + "h ago";
    }
    auto days = hours / 24;
    return std::to_string(days) + "d ago";
}

// ============================================================================
// String Helpers
// ============================================================================

std::string trim(const std::string& str) {
    auto start = std::find_if(str.begin(), str.end(), [](unsigned char c) {
        return !std::isspace(c);
    });
    auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && 
           str.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ============================================================================
// File Operations
// ============================================================================

Result<std::string> readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<std::string>::err("Cannot open file: " + path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Result<std::string>::ok(buffer.str());
}

Result<void> writeFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return Result<void>::err("Cannot open file for writing: " + path.string());
    }
    file << content;
    if (file.fail()) {
        return Result<void>::err("Failed to write to file: " + path.string());
    }
    return Result<void>::ok();
}

bool fileExists(const std::filesystem::path& path) {
    return fs::exists(path);
}

size_t fileSize(const std::filesystem::path& path) {
    if (!fs::exists(path)) return 0;
    return fs::file_size(path);
}

// ============================================================================
// PID File Implementation
// ============================================================================

PidFile::PidFile(const std::filesystem::path& path) : m_path(path) {}

PidFile::~PidFile() = default;

bool PidFile::write() {
    std::ofstream file(m_path);
    if (!file.is_open()) {
        return false;
    }
    file << getpid();
    return !file.fail();
}

std::optional<pid_t> PidFile::read(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    pid_t pid;
    file >> pid;
    if (file.fail()) {
        return std::nullopt;
    }
    return pid;
}

bool PidFile::isRunning(pid_t pid) {
    // Signal 0 checks if process exists without sending a signal
    return kill(pid, 0) == 0;
}

bool PidFile::remove() {
    return fs::remove(m_path);
}

// ============================================================================
// Initializer Implementation
// ============================================================================

bool Initializer::createDataDir() {
    auto dir = Paths::dataDir();
    return fs::create_directories(dir);
}

bool Initializer::createDefaultConfig() {
    auto dir = Paths::dataDir();
    auto configPath = dir / "config.json";
    
    if (fs::exists(configPath)) {
        return true; // Already exists
    }
    
    std::string defaultConfig = R"({
    "version": "1.0",
    "server": {
        "port": 8080,
        "address": "0.0.0.0",
        "workers": 0,
        "timeout": 30
    },
    "logging": {
        "level": "INFO",
        "file": "omninet.log",
        "toConsole": true,
        "toFile": true
    },
    "database": {
        "path": "omninet.db"
    },
    "cors": {
        "enabled": false,
        "origins": "*",
        "methods": "GET,POST,PUT,DELETE",
        "headers": "Content-Type,Authorization"
    },
    "tls": {
        "enabled": false,
        "certFile": "",
        "keyFile": ""
    }
})";
    
    std::ofstream file(configPath);
    if (!file.is_open()) {
        return false;
    }
    file << defaultConfig;
    return !file.fail();
}

bool Initializer::createCertsDir() {
    return fs::create_directories(Paths::certsDir());
}

bool Initializer::createServersDir() {
    return fs::create_directories(Paths::serversDir());
}

bool Initializer::setupAll() {
    if (!createDataDir()) return false;
    if (!createCertsDir()) return false;
    if (!createServersDir()) return false;
    if (!createDefaultConfig()) return false;
    return true;
}

// ============================================================================
// Version Helpers
// ============================================================================

void printVersion(const char* toolName) {
    std::cout << toolName << " v" << OMNINET_TOOLS_VERSION_STRING << "\n";
}

void printFullVersion(const char* toolName) {
    std::cout << toolName << " v" << OMNINET_TOOLS_VERSION_STRING << "\n";
    std::cout << "  Build: " << OMNINET_BUILD_TYPE << "\n";
    #ifdef OMNINET_GIT_COMMIT
    std::cout << "  Git: " << OMNINET_GIT_COMMIT << "\n";
    #endif
}

std::string getVersionString() {
    return OMNINET_TOOLS_VERSION_STRING;
}

std::string getFullVersionString() {
    std::string ver = OMNINET_TOOLS_VERSION_STRING;
    ver += " [" + std::string(OMNINET_BUILD_TYPE) + "]";
    return ver;
}

} // namespace OmniNet::Tools
