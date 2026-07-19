// OmniNet Tools - Common Utilities
#ifndef OMNINET_TOOLS_COMMON_H
#define OMNINET_TOOLS_COMMON_H

#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <variant>

// Include version information (version.h provides CMake-defined macros)
#include "version.h"

namespace OmniNet::Tools {

// ============================================================================
// Version Convenience Accessors
// ============================================================================

// Use macros from version.h
// These are defined by CMake compile definitions if available

// ============================================================================
// Exit Codes
// ============================================================================

enum class ExitCode {
    SUCCESS = 0,
    ERROR = 1,
    NOT_FOUND = 2,
    PERMISSION_DENIED = 3,
    INVALID_ARGUMENT = 4,
    TIMEOUT = 5,
    NETWORK_ERROR = 6
};

// ============================================================================
// Common Options (all tools support these)
// ============================================================================

struct ToolOptions {
    bool help = false;
    bool version = false;
    bool json = false;
    bool quiet = false;
    bool verbose = false;
    bool debug = false;
    std::optional<std::string> configPath;
};

// ============================================================================
// Path Helpers
// ============================================================================

class Paths {
public:
    // XDG Base Directory paths (see docs/xdg-standard.md)
    static std::filesystem::path configDir();
    static std::filesystem::path dataDir();
    static std::filesystem::path stateDir();
    static std::filesystem::path cacheDir();
    static std::filesystem::path runtimeDir();

    // Legacy single-tree root (~/.omninet or $OMNINET_DATA_DIR)
    static std::filesystem::path legacyDataDir();

    // Get config file path ($XDG_CONFIG_HOME/omninet/config.json)
    static std::filesystem::path configFile();
    
    // Get database path ($XDG_DATA_HOME/omninet/omninet.db)
    static std::filesystem::path databaseFile();
    
    // Get log file path ($XDG_STATE_HOME/omninet/omninet.log)
    static std::filesystem::path logFile();
    
    // Get certificates directory ($XDG_DATA_HOME/omninet/certs)
    static std::filesystem::path certsDir();
    
    // Get servers directory ($XDG_DATA_HOME/omninet/servers)
    static std::filesystem::path serversDir();
    
    // Resolve path relative to data dir
    static std::filesystem::path resolve(const std::string& filename);
    
    // Expand ~ to home directory
    static std::string expandHome(const std::string& path);
};

// ============================================================================
// Time Helpers
// ============================================================================

std::string formatTimestamp(time_t timestamp);
std::string formatDuration(std::chrono::seconds duration);
std::string timeAgo(std::chrono::system_clock::time_point time);

// ============================================================================
// String Helpers
// ============================================================================

std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);

// ============================================================================
// Result Type (for error handling)
// ============================================================================

template<typename T>
struct Result {
    bool success;
    T value;
    std::string error;
    
    static Result ok(T val) { return {true, std::move(val), {}}; }
    static Result err(const std::string& err) { return {false, {}, err}; }
    
    explicit operator bool() const { return success; }
    const T& operator*() const { return value; }
    const T* operator->() const { return &value; }
};

template<>
struct Result<void> {
    bool success;
    std::string error;
    
    static Result ok() { return {true, {}}; }
    static Result err(const std::string& err) { return {false, err}; }
    
    explicit operator bool() const { return success; }
};

// ============================================================================
// File Operations
// ============================================================================

Result<std::string> readFile(const std::filesystem::path& path);
Result<void> writeFile(const std::filesystem::path& path, const std::string& content);
bool fileExists(const std::filesystem::path& path);
size_t fileSize(const std::filesystem::path& path);

// ============================================================================
// Process ID File (PID file management)
// ============================================================================

class PidFile {
public:
    explicit PidFile(const std::filesystem::path& path);
    ~PidFile();
    
    // Write current process PID
    bool write();
    
    // Read PID from file
    static std::optional<pid_t> read(const std::filesystem::path& path);
    
    // Check if process is running
    static bool isRunning(pid_t pid);
    
    // Remove PID file
    bool remove();
    
private:
    std::filesystem::path m_path;
};

// ============================================================================
// Version Helpers
// ============================================================================

// Print version: "toolname vX.Y.Z"
void printVersion(const char* toolName);

// Print full version with build info
void printFullVersion(const char* toolName);

// Get version string
std::string getVersionString();

// Get full version string with build type
std::string getFullVersionString();

// ============================================================================
// Initializer (for setting up the OmniNet XDG directories)
// ============================================================================

class Initializer {
public:
    // Create the data directory ($XDG_DATA_HOME/omninet)
    static bool createDataDir();
    
    // Create default config.json under $XDG_CONFIG_HOME/omninet
    static bool createDefaultConfig();
    
    // Create certificates directory
    static bool createCertsDir();
    
    // Create servers directory
    static bool createServersDir();
    
    // Create all default directories (config/data/state/cache/runtime)
    static bool setupAll();
};

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_COMMON_H
