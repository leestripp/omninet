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
#include "tools_common/database.h"
#include "tools_common/password.h"

namespace fs = std::filesystem;
using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-init";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    bool force = false;
    bool nonInteractive = false;
    bool skipCerts = false;
    bool skipAdmin = false;
    std::string port = "8080";
    std::string address = "0.0.0.0";
    std::string dataDir;
    std::string adminPassword;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Interactive Input
// ============================================================================

std::string getPassword(const std::string& prompt);

std::string ask(const std::string& prompt, const std::string& defaultValue = {});
bool confirm(const std::string& prompt, bool defaultValue = false);

// ============================================================================
// Setup Functions
// ============================================================================

bool setupDataDirectory(const Options& opts, Formatter& out);
bool setupConfig(const Options& opts, Formatter& out);
bool setupCertificates(const Options& opts, Formatter& out);
bool setupAdminUser(const Options& opts, Formatter& out);

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
    auto configPath = Paths::configFile();
    
    if (fs::exists(configPath)) {
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
    if (!opts.skipCerts) {
        if (!setupCertificates(opts, out)) {
            out.warning("Certificate generation skipped or failed");
        }
    } else {
        out.info("Skipping certificate generation (--skip-certs)");
    }
    
    // Step 4: Create admin user
    if (!opts.skipAdmin) {
        if (!setupAdminUser(opts, out)) {
            out.warning("Admin user creation skipped");
        }
    } else {
        out.info("Skipping admin user creation (--skip-admin)");
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
            std::cout << "  -y, --yes           Non-interactive mode (skip prompts)\n";
            std::cout << "  -p, --port          Default port (default: 8080)\n";
            std::cout << "  -a, --address       Default address (default: 0.0.0.0)\n";
            std::cout << "  -d, --data-dir      Data directory path\n";
            std::cout << "  --skip-certs        Skip certificate generation\n";
            std::cout << "  --skip-admin        Skip admin user creation\n";
            std::cout << "  --admin-password    Set admin password (non-interactive)\n";
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
        else if (arg == "--skip-certs") {
            opts.skipCerts = true;
        }
        else if (arg == "--skip-admin") {
            opts.skipAdmin = true;
        }
        else if (arg == "--admin-password") {
            if (i + 1 < argc) {
                opts.adminPassword = argv[++i];
            }
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
    
    if (fs::exists(certPath) && fs::exists(keyPath)) {
        out.warning("Certificate already exists: " + certPath.string());
        
        if (!opts.nonInteractive && !confirm("Regenerate?", false)) {
            out.info("Keeping existing certificate");
            return true;
        }
    }
    
    // Get common name
    std::string commonName = "localhost";
    if (!opts.nonInteractive) {
        auto input = ask("Common name for certificate", commonName);
        if (!input.empty()) commonName = input;
    }
    
    out.info("Generating self-signed certificate for: " + commonName);
    
    // Generate certificate using OpenSSL via command
    // We'll create a shell command to generate the certificate
    std::string cmd = "openssl req -x509 -newkey rsa:4096 -keyout \"" + keyPath.string() + 
                     "\" -out \"" + certPath.string() + 
                     "\" -days 365 -nodes -subj \"/CN=" + commonName + "\" 2>&1";
    
    // Show progress message while generating certificate
    std::cout << "  Generating certificate... " << std::flush;
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cout << "FAILED\n";
        out.error("Failed to run OpenSSL");
        return false;
    }
    
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int status = pclose(pipe);
    
    if (status == 0 && fs::exists(certPath) && fs::exists(keyPath)) {
        std::cout << "OK\n";
        out.success("Certificate generated successfully!");
        out.kv("Certificate", certPath.string());
        out.kv("Private key", keyPath.string());
        out.info("Note: This is a self-signed certificate for development use only.");
        return true;
    } else {
        std::cout << "FAILED\n";
        out.error("Certificate generation failed");
        if (!output.empty()) {
            out.text("OpenSSL output: " + output);
        }
        // Clean up partial files
        if (fs::exists(certPath)) fs::remove(certPath);
        if (fs::exists(keyPath)) fs::remove(keyPath);
        return false;
    }
}

bool setupAdminUser(const Options& opts, Formatter& out) {
    out.subheader("Creating Admin User");
    
    // Ensure database exists
    auto dbPath = Paths::databaseFile();
    
    Database db;
    if (!db.open(dbPath.string())) {
        out.error("Failed to open database: " + db.lastError());
        return false;
    }
    
    // Ensure users table exists
    UserStore store(db);
    if (!store.ensureTables()) {
        out.error("Failed to create user tables");
        return false;
    }
    
    // Check if admin user already exists
    auto admin = store.getUserByUsername("admin");
    if (admin) {
        out.warning("Admin user already exists");
        out.info("Use 'omninet-users' tool to manage users");
        return true;
    }
    
    // Get admin credentials
    out.info("Creating admin user 'admin'");
    std::cout << '\n';
    
    std::string password1;
    
    // Use provided password or prompt
    if (!opts.adminPassword.empty()) {
        password1 = opts.adminPassword;
    } else if (opts.nonInteractive) {
        out.warning("No admin password provided - skipping admin user creation");
        out.info("Use --admin-password <password> to set a password");
        return true;
    } else {
        password1 = getPassword("Enter password for admin user");
    }
    
    if (password1.empty()) {
        out.warning("Password cannot be empty - skipping admin user creation");
        return true;
    }
    
    std::string password2;
    if (!opts.adminPassword.empty()) {
        password2 = opts.adminPassword;
    } else if (!opts.nonInteractive) {
        password2 = getPassword("Confirm password");
    } else {
        password2 = password1;
    }
    
    if (password1 != password2) {
        out.error("Passwords do not match");
        return false;
    }
    
    // Hash password
    std::string passwordHash = OmniNet::Tools::hashPassword(password1);
    if (passwordHash.empty()) {
        out.error("Failed to hash password");
        return false;
    }
    
    // Create user
    if (!store.createUser("admin", passwordHash, "admin")) {
        out.error("Failed to create admin user: " + db.lastError());
        return false;
    }
    
    out.success("Admin user created successfully!");
    out.info("Username: admin");
    out.info("Role: admin");
    out.warning("Please change this password after first login!");
    
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
