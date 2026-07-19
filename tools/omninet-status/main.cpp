// omninet-status - Overall System Status
// Quick diagnostic tool for OmniNet servers

#include <iostream>
#include <memory>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <ctime>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"
#include "tools_common/config.h"
#include "tools_common/database.h"

#include <nlohmann/json.hpp>

using namespace OmniNet::Tools;
using json = nlohmann::json;

namespace fs = std::filesystem;

constexpr const char* TOOL_NAME = "omninet-status";

// ============================================================================
// Configuration
// ============================================================================

struct StatusOptions {
    bool verbose = false;
    bool json = false;
    bool report = false;
};

StatusOptions parseArgs(int argc, char* argv[]);

// ============================================================================
// Status Checks
// ============================================================================

bool checkConfig(const StatusOptions& opts, Formatter& out);
bool checkDatabase(const StatusOptions& opts, Formatter& out);
bool checkLogs(const StatusOptions& opts, Formatter& out);
bool checkCerts(const StatusOptions& opts, Formatter& out);
bool checkBlockedIPs(const StatusOptions& opts, Formatter& out);
bool checkIntrusion(const StatusOptions& opts, Formatter& out);

// ============================================================================
// JSON Output
// ============================================================================

void printJsonOutput();

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    // Show version if requested
    if (opts.json) {
        printJsonOutput();
        return 0;
    }
    
    // Determine if terminal supports colors
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    std::cout << '\n';
    out.header("OmniNet System Status");
    std::cout << '\n';
    
    // System info
    out.kv("Config Directory", Paths::configDir().string());
    out.kv("Config File", Paths::configFile().string());
    out.kv("Data Directory", Paths::dataDir().string());
    out.kv("Database File", Paths::databaseFile().string());
    out.kv("State Directory", Paths::stateDir().string());
    out.kv("Log File", Paths::logFile().string());
    out.kv("Cache Directory", Paths::cacheDir().string());
    out.kv("Runtime Directory", Paths::runtimeDir().string());
    std::cout << '\n';
    
    // Run checks
    bool allOk = true;
    
    allOk &= checkConfig(opts, out);
    allOk &= checkDatabase(opts, out);
    allOk &= checkCerts(opts, out);
    allOk &= checkBlockedIPs(opts, out);
    allOk &= checkIntrusion(opts, out);
    allOk &= checkLogs(opts, out);
    
    std::cout << '\n';
    
    if (allOk) {
        out.success("All checks passed");
    } else {
        out.warning("Some checks failed - see details above");
    }
    
    std::cout << '\n';
    
    return allOk ? 0 : 1;
}

// ============================================================================
// Argument Parsing
// ============================================================================

StatusOptions parseArgs(int argc, char* argv[]) {
    StatusOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << TOOL_NAME << " - OmniNet System Status Tool\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " [OPTIONS]\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help      Show this help message\n";
            std::cout << "  -V, --version   Show version information\n";
            std::cout << "  -v, --verbose   Show detailed output\n";
            std::cout << "  -j, --json      Output as JSON\n";
            std::cout << "  -r, --report    Generate full diagnostic report\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-r" || arg == "--report") {
            opts.report = true;
        }
    }
    
    return opts;
}

// ============================================================================
// Configuration Check
// ============================================================================

bool checkConfig(const StatusOptions& opts, Formatter& out) {
    out.subheader("Configuration");
    
    auto configPath = Paths::configFile();
    if (!fs::exists(configPath)) {
        out.error("Config file not found: " + configPath.string());
        return false;
    }
    
    out.success("Config file exists");
    
    ConfigReader reader;
    if (!reader.isLoaded()) {
        out.error("Failed to load config file");
        return false;
    }
    
    const auto& cfg = reader.config();
    
    out.kv("Version", cfg.version);
    out.kv("Server Port", std::to_string(cfg.server.port));
    out.kv("Server Address", cfg.server.address);
    out.kv("Log Level", cfg.logging.level);
    out.kv("TLS Enabled", cfg.tls.enabled ? "yes" : "no");
    out.kv("Dashboard Enabled", cfg.dashboard.enabled ? "yes" : "no");
    out.kv("Intrusion Detection", cfg.intrusion.enabled ? "enabled" : "disabled");
    
    if (opts.verbose) {
        out.kv("CORS Enabled", cfg.cors.enabled ? "yes" : "no");
        out.kv("Max Failed Attempts", std::to_string(cfg.intrusion.maxFailedAttempts));
        out.kv("Block Duration", std::to_string(cfg.intrusion.blockDuration) + " minutes");
    }
    
    std::cout << '\n';
    return true;
}

// ============================================================================
// Database Check
// ============================================================================

bool checkDatabase(const StatusOptions& opts, Formatter& out) {
    out.subheader("Database");
    
    auto dbPath = Paths::databaseFile();
    if (!fs::exists(dbPath)) {
        out.error("Database not found: " + dbPath.string());
        return false;
    }
    
    out.success("Database exists");
    
    // Check size
    auto size = fs::file_size(dbPath);
    double sizeKb = static_cast<double>(size) / 1024.0;
    out.kv("Size", sizeKb < 1024 ? 
           std::to_string(static_cast<int>(sizeKb)) + " KB" :
           std::to_string(static_cast<int>(sizeKb / 1024)) + " MB");
    
    // Open and check
    Database db;
    if (!db.open(dbPath.string())) {
        out.error("Failed to open database: " + db.lastError());
        return false;
    }
    
    // List tables
    auto tables = db.listTables();
    out.kv("Tables", std::to_string(tables.size()));
    
    if (opts.verbose) {
        for (const auto& table : tables) {
            out.text("  - " + table, Style::DIM_WHITE);
        }
    }
    
    // Check for key tables
    bool hasUsers = false, hasSessions = false, hasIntrusion = false;
    for (const auto& t : tables) {
        if (t == "users") hasUsers = true;
        else if (t == "sessions") hasSessions = true;
        else if (t == "intrusion_logs") hasIntrusion = true;
    }
    
    if (hasUsers) out.success("Users table exists");
    else out.warning("Users table not found");
    
    if (hasSessions) out.success("Sessions table exists");
    else out.warning("Sessions table not found");
    
    if (hasIntrusion) out.success("Intrusion logs table exists");
    else out.warning("Intrusion logs table not found");
    
    std::cout << '\n';
    return hasUsers && hasSessions;
}

// ============================================================================
// Certificate Check
// ============================================================================

bool checkCerts(const StatusOptions& opts, Formatter& out) {
    out.subheader("TLS Certificates");
    
    auto certsDir = Paths::certsDir();
    if (!fs::exists(certsDir)) {
        out.warning("Certificates directory not found: " + certsDir.string());
        return false;
    }
    
    // Count certificates
    int certCount = 0, keyCount = 0;
    std::vector<std::string> certFiles, keyFiles;
    
    for (const auto& entry : fs::directory_iterator(certsDir)) {
        auto ext = entry.path().extension().string();
        if (ext == ".crt" || ext == ".pem" || ext == ".cer") {
            certFiles.push_back(entry.path().filename());
            certCount++;
        }
        else if (ext == ".key" || ext == ".pem") {
            keyFiles.push_back(entry.path().filename());
            keyCount++;
        }
    }
    
    if (certCount == 0) {
        out.warning("No certificates found");
        return false;
    }
    
    out.success(std::to_string(certCount) + " certificate(s) found");
    out.kv("Key files", std::to_string(keyCount));
    
    if (opts.verbose) {
        for (const auto& cert : certFiles) {
            out.text("  Certificate: " + cert, Style::DIM_WHITE);
        }
        for (const auto& key : keyFiles) {
            out.text("  Key: " + key, Style::DIM_WHITE);
        }
    }
    
    // Check config TLS settings
    ConfigReader reader;
    if (reader.config().tls.enabled) {
        if (!reader.config().tls.certFile.empty()) {
            auto certPath = Paths::resolve(reader.config().tls.certFile);
            if (fs::exists(certPath)) {
                out.success("Configured cert exists: " + certPath.filename().string());
            } else {
                out.warning("Configured cert not found: " + certPath.string());
            }
        }
    }
    
    std::cout << '\n';
    return true;
}

// ============================================================================
// Blocked IPs Check
// ============================================================================

bool checkBlockedIPs(const StatusOptions& opts, Formatter& out) {
    out.subheader("Blocked IPs");
    
    auto dbPath = Paths::databaseFile();
    if (!fs::exists(dbPath)) {
        out.warning("Cannot check blocked IPs - database not found");
        return false;
    }
    
    Database db;
    if (!db.open(dbPath.string())) {
        out.warning("Cannot open database for blocked IP check");
        return false;
    }
    
    IntrusionStore store(db);
    auto blocked = store.getBlockedIPs();
    auto stats = store.stats();
    
    if (blocked.empty()) {
        out.success("No blocked IPs");
    } else {
        out.warning(std::to_string(blocked.size()) + " blocked IP(s)");
        
        if (opts.verbose) {
            for (const auto& b : blocked) {
                auto expiresTime = std::chrono::system_clock::to_time_t(b.expires);
                out.text("  " + b.ip + " (expires: " + formatTimestamp(expiresTime) + ")", Style::DIM_WHITE);
            }
        }
    }
    
    out.kv("Total intrusion logs", std::to_string(stats.totalLogs));
    
    std::cout << '\n';
    return true;
}

// ============================================================================
// Intrusion Check
// ============================================================================

bool checkIntrusion(const StatusOptions& opts, Formatter& out) {
    out.subheader("Intrusion Detection");
    
    ConfigReader reader;
    const auto& cfg = reader.config();
    
    if (!cfg.intrusion.enabled) {
        out.warning("Intrusion detection is disabled");
        std::cout << '\n';
        return true;
    }
    
    out.success("Intrusion detection is enabled");
    
    if (cfg.intrusion.autoBlock) {
        out.success("Auto-blocking is enabled");
    } else {
        out.warning("Auto-blocking is disabled");
    }
    
    out.kv("Max Failed Attempts", std::to_string(cfg.intrusion.maxFailedAttempts));
    out.kv("Block Duration", std::to_string(cfg.intrusion.blockDuration) + " minutes");
    out.kv("Attempt Window", std::to_string(cfg.intrusion.attemptWindow) + " minutes");
    
    // Show recent suspicious activity
    auto dbPath = Paths::databaseFile();
    if (fs::exists(dbPath)) {
        Database db;
        if (db.open(dbPath.string())) {
            IntrusionStore store(db);
            auto logs = store.getRecentLogs(5);
            
            if (!logs.empty()) {
                std::cout << '\n';
                out.text("Recent suspicious activity:", Style::KEY);
                for (const auto& log : logs) {
                    auto ts = std::chrono::system_clock::to_time_t(log.timestamp);
                    out.text("  " + formatTimestamp(ts) + " | " + 
                             log.clientIp + " | " + log.method + " " + log.path, 
                             Style::DIM_WHITE);
                }
            }
        }
    }
    
    std::cout << '\n';
    return true;
}

// ============================================================================
// Log Check
// ============================================================================

bool checkLogs(const StatusOptions& opts, Formatter& out) {
    out.subheader("Log File");
    
    auto logPath = Paths::logFile();
    if (!fs::exists(logPath)) {
        out.warning("Log file not found: " + logPath.string());
        return false;
    }
    
    auto size = fs::file_size(logPath);
    double sizeKb = static_cast<double>(size) / 1024.0;
    out.kv("Size", sizeKb < 1024 ? 
           std::to_string(static_cast<int>(sizeKb)) + " KB" :
           std::to_string(static_cast<int>(sizeKb / 1024)) + " MB");
    
    // Read last few lines
    if (opts.verbose || opts.report) {
        std::ifstream file(logPath);
        if (file.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            
            // Read last 10 lines
            while (std::getline(file, line)) {
                lines.push_back(line);
                if (lines.size() > 10) {
                    lines.erase(lines.begin());
                }
            }
            
            if (!lines.empty()) {
                std::cout << '\n';
                out.text("Recent log entries:", Style::KEY);
                for (const auto& l : lines) {
                    std::cout << "  " << l << '\n';
                }
            }
        }
    }
    
    std::cout << '\n';
    return true;
}

// ============================================================================
// JSON Output
// ============================================================================

void printJsonOutput() {
    json result;
    result["status"] = "ok";
    result["version"] = getVersionString();
    
    // Configuration status
    auto configPath = Paths::configFile();
    result["config"]["exists"] = fs::exists(configPath);
    
    if (result["config"]["exists"]) {
        ConfigReader reader;
        if (reader.isLoaded()) {
            const auto& cfg = reader.config();
            result["config"]["loaded"] = true;
            result["config"]["version"] = cfg.version;
            result["config"]["server"]["port"] = cfg.server.port;
            result["config"]["server"]["address"] = cfg.server.address;
            result["config"]["tls"]["enabled"] = cfg.tls.enabled;
            result["config"]["dashboard"]["enabled"] = cfg.dashboard.enabled;
            result["config"]["intrusion"]["enabled"] = cfg.intrusion.enabled;
        } else {
            result["config"]["loaded"] = false;
        }
    } else {
        result["config"]["loaded"] = false;
    }
    
    // Database status
    auto dbPath = Paths::databaseFile();
    result["database"]["exists"] = fs::exists(dbPath);
    result["database"]["path"] = dbPath.string();
    
    // Log file status
    auto logPath = Paths::logFile();
    result["logs"]["path"] = logPath.string();
    result["logs"]["exists"] = fs::exists(logPath);
    if (result["logs"]["exists"]) {
        try {
            auto size = fs::file_size(logPath);
            result["logs"]["size_bytes"] = size;
        } catch (...) {
            result["logs"]["size_bytes"] = 0;
        }
    }
    
    // Paths
    result["paths"]["config_dir"] = Paths::configDir().string();
    result["paths"]["config_file"] = configPath.string();
    result["paths"]["data_dir"] = Paths::dataDir().string();
    result["paths"]["database_file"] = dbPath.string();
    result["paths"]["state_dir"] = Paths::stateDir().string();
    result["paths"]["log_file"] = logPath.string();
    result["paths"]["cache_dir"] = Paths::cacheDir().string();
    result["paths"]["runtime_dir"] = Paths::runtimeDir().string();
    
    std::cout << result.dump(2) << "\n";
}
