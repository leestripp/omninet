// omninet-init - OmniNet Initialization Tool
// Set up the ~/.omninet directory structure and default configuration

#include <iostream>
#include <unistd.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <termios.h>
#include <unistd.h>
#include <regex>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/config.h"

namespace fs = std::filesystem;
using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-init";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    bool force = false;
    bool nonInteractive = false;
    std::string port = "8080";
    std::string address = "0.0.0.0";
    std::string dataDir;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Interactive Input
// ============================================================================

std::string ask(const std::string& prompt, const std::string& defaultValue = {});
bool confirm(const std::string& prompt, bool defaultValue = false);
std::string getPassword(const std::string& prompt);

// ============================================================================
// Setup Functions
// ============================================================================

bool setupDataDirectory(const Options& opts, Formatter& out);
bool setupConfig(const Options& opts, Formatter& out);
bool setupCertificates(const Options& opts, Formatter& out);
bool setupAdminUser(Formatter& out);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    std::cout << "\n";
    out.header("OmniNet Initialization");
    std::cout << '\n';
    
    // Check if already initialized
    auto dataDir = Paths::dataDir();
    
    if (fs::exists(dataDir)) {
        if (!opts.force) {
            out.warning("OmniNet is already initialized at: " + dataDir.string());
            out.info("Use --force to reinitialize");
            return 0;
        }
        out.warning("Reinitializing OmniNet (--force specified)");
    }
    
    // Step 1: Create directories
    if (!setupDataDirectory(opts, out)) {
        out.error("Failed to create data directory");
        return 1;
    }
    
    // Step 2: Create config
    if (!setupConfig(opts, out)) {
        out.error("Failed to create configuration");
        return 1;
    }
    
    // Step 3: Generate certificates
    if (!setupCertificates(opts, out)) {
        out.warning("Certificate generation skipped or failed");
    }
    
    // Step 4: Create admin user
    if (!setupAdminUser(out)) {
        out.warning("Admin user creation skipped");
    }
    
    // Summary
    std::cout << '\n';
    out.header("Setup Complete");
    
    out.success("OmniNet has been initialized successfully!");
    std::cout << '\n';
    
    out.kv("Data Directory", dataDir.string());
    out.kv("Config File", Paths::configFile().string());
    out.kv("Database", Paths::databaseFile().string());
    out.kv("Logs", Paths::logFile().string());
    out.kv("Certificates", Paths::certsDir().string());
    std::cout << '\n';
    
    out.info("Next steps:");
    out.text("  1. Review config: omninet-status");
    out.text("  2. Start a server: omninet-ctl start myserver");
    out.text("  3. Access dashboard: http://localhost:" + opts.port + "/omninet");
    std::cout << '\n';
    
    return 0;
}

// ============================================================================
// Argument Parsing
// ============================================================================

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << TOOL_NAME << " - OmniNet Initialization Tool\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " [options]\n";
            std::cout << '\n';
            std::cout << "Initialize OmniNet for first-time use.\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help          Show this help\n";
            std::cout << "  -V, --version       Show version information\n";
            std::cout << "  -f, --force         Force reinitialization\n";
            std::cout << "  -y, --yes           Non-interactive mode\n";
            std::cout << "  -p, --port          Default port (default: 8080)\n";
            std::cout << "  -a, --address       Default address (default: 0.0.0.0)\n";
            std::cout << "  -d, --data-dir      Data directory path\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "-f" || arg == "--force") {
            opts.force = true;
        }
        else if (arg == "-y" || arg == "--yes" || arg == "--non-interactive") {
            opts.nonInteractive = true;
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                opts.port = argv[++i];
            }
        }
        else if (arg == "-a" || arg == "--address") {
            if (i + 1 < argc) {
                opts.address = argv[++i];
            }
        }
        else if (arg == "-d" || arg == "--data-dir") {
            if (i + 1 < argc) {
                opts.dataDir = argv[++i];
            }
        }
    }
    
    return opts;
}

// ============================================================================
// Setup Functions
// ============================================================================

bool setupDataDirectory(const Options& opts, Formatter& out) {
    out.subheader("Creating Data Directory");
    
    auto dataDir = Paths::dataDir();
    
    if (!opts.dataDir.empty()) {
        dataDir = opts.dataDir;
    }
    
    out.info("Creating: " + dataDir.string());
    
    std::error_code ec;
    fs::create_directories(dataDir, ec);
    
    if (ec) {
        out.error("Failed: " + ec.message());
        return false;
    }
    
    fs::create_directories(dataDir / "certs", ec);
    if (ec) {
        out.warning("Failed to create certs directory");
    }
    
    fs::create_directories(dataDir / "servers", ec);
    if (ec) {
        out.warning("Failed to create servers directory");
    }
    
    out.success("Data directory created");
    return true;
}

bool setupConfig(const Options& opts, Formatter& out) {
    out.subheader("Creating Configuration");
    
    auto configPath = Paths::configFile();
    
    out.info("Creating: " + configPath.string());
    
    std::string port = opts.port;
    std::string address = opts.address;
    
    if (!opts.nonInteractive) {
        auto input = ask("Default port", port);
        if (!input.empty()) port = input;
        
        input = ask("Default address", address);
        if (!input.empty()) address = input;
    }
    
    std::string config = R"({
    "version": "1.0",
    "server": {
        "address": ")" + address + R"(",
        "port": )" + port + R"(,
        "workers": 0,
        "timeout": 30
    },
    "logging": {
        "level": "INFO",
        "file": "omninet.log",
        "toConsole": true,
        "toFile": true,
        "maxSizeMb": 10,
        "maxFiles": 5
    },
    "database": {
        "path": "omninet.db",
        "busyTimeoutMs": 5000,
        "foreignKeys": true,
        "walMode": true
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
    },
    "dashboard": {
        "enabled": true,
        "urlPath": "/omninet",
        "apiPath": "/omninet/api",
        "sessionTtl": 3600,
        "maxLoginAttempts": 5
    },
    "intrusion": {
        "enabled": true,
        "autoBlock": true,
        "blockDuration": 30,
        "maxFailedAttempts": 10,
        "attemptWindow": 15
    }
})";
    
    std::ofstream file(configPath);
    if (!file.is_open()) {
        out.error("Failed to create config file");
        return false;
    }
    
    file << config;
    file.close();
    
    out.success("Configuration created");
    return true;
}

bool setupCertificates(const Options& opts, Formatter& out) {
    out.subheader("Generating TLS Certificates");
    
    auto certsDir = Paths::certsDir();
    auto certPath = certsDir / "omninet.crt";
    auto keyPath = certsDir / "omninet.key";
    
    if (fs::exists(certPath)) {
        out.warning("Certificate already exists: " + certPath.string());
        
        if (!opts.nonInteractive && !confirm("Regenerate?", false)) {
            out.info("Keeping existing certificate");
            return true;
        }
    }
    
    out.info("Generating self-signed certificate...");
    out.info("Common name [localhost]: ");
    
    std::string commonName = "localhost";
    if (!opts.nonInteractive) {
        std::getline(std::cin, commonName);
        if (commonName.empty()) commonName = "localhost";
    }
    
    // Use OpenSSL to generate certificate
    // This is a simplified version - full implementation would use OpenSSL API
    
    out.info("Certificate generation requires OpenSSL integration");
    out.info("Use: openssl req -x509 -newkey rsa:4096 -keyout " + keyPath.string() + 
           " -out " + certPath.string() + " -days 365 -nodes");
    
    // For now, just create placeholder
    out.warning("Please generate certificates manually or use scripts/generate-certs.sh");
    
    return false;
}

bool setupAdminUser(Formatter& out) {
    out.subheader("Creating Admin User");
    
    out.info("Admin user creation requires database access");
    out.info("Use the web dashboard at /omninet to create users");
    
    // TODO: Implement actual user creation with database
    
    return true;
}

// ============================================================================
// Interactive Input
// ============================================================================

std::string ask(const std::string& prompt, const std::string& defaultValue) {
    std::cout << prompt;
    if (!defaultValue.empty()) {
        std::cout << " [" << defaultValue << "]";
    }
    std::cout << ": ";
    std::cout << std::flush;
    
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    return input;
}

bool confirm(const std::string& prompt, bool defaultValue) {
    std::string suffix = defaultValue ? " [Y/n]: " : " [y/N]: ";
    std::cout << prompt << suffix << std::flush;
    
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    return (input[0] == 'y' || input[0] == 'Y');
}

std::string getPassword(const std::string& prompt) {
    std::cout << prompt << ": " << std::flush;
    
    struct termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    std::string password;
    std::getline(std::cin, password);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << '\n';
    
    return password;
}
