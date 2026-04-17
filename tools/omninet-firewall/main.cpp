// omninet-firewall - Intrusion Detection Management Tool
// Manage blocked IPs, view logs, configure rules

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"
#include "tools_common/database.h"

using namespace OmniNet::Tools;

namespace fs = std::filesystem;

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string action;           // list, block, unblock, logs, rules, whitelist
    std::string target;           // IP address or rule name
    std::string duration;         // Block duration (e.g., "30m", "1h", "1d")
    std::string since;            // Time filter for logs
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Duration Parsing
// ============================================================================

std::chrono::minutes parseDuration(const std::string& str);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    // Check database exists
    auto dbPath = Paths::databaseFile();
    if (!fs::exists(dbPath)) {
        std::cerr << "Error: Database not found: " << dbPath.string() << '\n';
        std::cerr << "This may indicate that OmniNet hasn't been initialized yet.\n";
        return 1;
    }
    
    // Open database
    Database db;
    if (!db.open(dbPath.string())) {
        std::cerr << "Error: Cannot open database: " << db.lastError() << '\n';
        return 1;
    }
    
    IntrusionStore store(db);
    
    // Execute action
    if (opts.action == "list" || opts.action == "blocked") {
        out.header("Blocked IPs");
        
        auto blocked = store.getBlockedIPs();
        if (blocked.empty()) {
            out.success("No blocked IPs");
            return 0;
        }
        
        // Table
        Table table;
        table.addColumn("IP Address", 20);
        table.addColumn("Blocked At", 20);
        table.addColumn("Expires In", 15);
        table.addColumn("Duration", 10);
        
        for (const auto& b : blocked) {
            auto blockedAt = std::chrono::system_clock::to_time_t(b.blockedAt);
            auto expires = std::chrono::system_clock::to_time_t(b.expires);
            auto now = std::chrono::system_clock::now();
            
            std::string expiresIn;
            if (b.expires > now) {
                auto remaining = std::chrono::duration_cast<std::chrono::minutes>(b.expires - now);
                if (remaining.count() >= 60) {
                    expiresIn = std::to_string(remaining.count() / 60) + "h " + 
                               std::to_string(remaining.count() % 60) + "m";
                } else {
                    expiresIn = std::to_string(remaining.count()) + "m";
                }
            } else {
                expiresIn = "expired";
            }
            
            auto blockedDuration = std::chrono::duration_cast<std::chrono::minutes>(b.expires - b.blockedAt);
            std::string duration;
            if (blockedDuration.count() >= 60) {
                duration = std::to_string(blockedDuration.count() / 60) + "h";
            } else {
                duration = std::to_string(blockedDuration.count()) + "m";
            }
            
            table.addRow({
                b.ip,
                formatTimestamp(blockedAt),
                expiresIn,
                duration
            });
        }
        
        table.render(std::cout);
        std::cout << '\n';
        out.kv("Total blocked", blocked.size());
        
    } else if (opts.action == "block") {
        if (opts.target.empty()) {
            std::cerr << "Error: IP address required\n";
            std::cerr << "Usage: omninet-firewall block <ip> [--duration 30m]\n";
            return 1;
        }
        
        auto duration = opts.duration.empty() ? std::chrono::minutes(30) : parseDuration(opts.duration);
        
        if (store.blockIP(opts.target, duration)) {
            out.success("Blocked " + opts.target + " for " + opts.duration);
        } else {
            out.error("Failed to block IP");
            return 1;
        }
        
    } else if (opts.action == "unblock") {
        if (opts.target == "all") {
            if (store.unblockAll()) {
                out.success("Unblocked all IPs");
            } else {
                out.error("Failed to unblock all IPs");
                return 1;
            }
        } else if (!opts.target.empty()) {
            if (store.unblockIP(opts.target)) {
                out.success("Unblocked " + opts.target);
            } else {
                out.error("Failed to unblock IP");
                return 1;
            }
        } else {
            std::cerr << "Error: IP address or '--all' required\n";
            return 1;
        }
        
    } else if (opts.action == "logs" || opts.action == "log") {
        out.header("Intrusion Logs");
        
        size_t limit = 50;
        auto logs = store.getRecentLogs(limit);
        
        if (logs.empty()) {
            out.info("No intrusion logs");
            return 0;
        }
        
        Table table;
        table.addColumn("Time", 20);
        table.addColumn("IP", 18);
        table.addColumn("Method", 8);
        table.addColumn("Path", 30);
        table.addColumn("Reason", 20);
        
        for (const auto& log : logs) {
            auto ts = std::chrono::system_clock::to_time_t(log.timestamp);
            
            std::string path = log.path;
            if (path.length() > 28) {
                path = path.substr(0, 25) + "...";
            }
            
            std::string reason = log.reason;
            if (reason.length() > 18) {
                reason = reason.substr(0, 15) + "...";
            }
            
            table.addRow({
                formatTimestamp(ts),
                log.clientIp,
                log.method,
                path,
                reason
            });
        }
        
        table.render(std::cout);
        std::cout << '\n';
        out.kv("Showing", std::to_string(logs.size()) + " entries");
        
    } else if (opts.action == "rules") {
        // TODO: Implement rules listing
        out.info("Rules management not yet implemented");
        
    } else if (opts.action == "whitelist") {
        // TODO: Implement whitelist
        out.info("Whitelist management not yet implemented");
        
    } else {
        // Default: show help
        std::cout << "Usage: omninet-firewall <command> [options]\n";
        std::cout << '\n';
        std::cout << "Commands:\n";
        std::cout << "  list, blocked       List currently blocked IPs\n";
        std::cout << "  block <ip>          Block an IP address\n";
        std::cout << "  unblock <ip|--all>  Unblock an IP or all IPs\n";
        std::cout << "  logs                Show recent intrusion logs\n";
        std::cout << "  rules               Manage detection rules\n";
        std::cout << "  whitelist           Manage IP whitelist\n";
        std::cout << '\n';
        std::cout << "Options:\n";
        std::cout << "  -h, --help          Show this help\n";
        std::cout << "  -d, --duration      Block duration (e.g., 30m, 1h, 1d)\n";
        std::cout << "  -j, --json          Output as JSON\n";
        std::cout << "  -v, --verbose       Verbose output\n";
    }
    
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
            std::cout << "Usage: omninet-firewall <command> [options]\n";
            std::cout << '\n';
            std::cout << "Intrusion detection management for OmniNet.\n";
            std::cout << '\n';
            std::cout << "Commands:\n";
            std::cout << "  list, blocked       List currently blocked IPs\n";
            std::cout << "  block <ip>          Block an IP address\n";
            std::cout << "  unblock <ip|--all> Unblock an IP or all IPs\n";
            std::cout << "  logs                Show recent intrusion logs\n";
            std::cout << "  rules               Manage detection rules\n";
            std::cout << "  whitelist           Manage IP whitelist\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help          Show this help\n";
            std::cout << "  -d, --duration      Block duration (e.g., 30m, 1h, 1d)\n";
            std::cout << "  -s, --since         Show logs since timestamp\n";
            std::cout << "  -j, --json          Output as JSON\n";
            std::cout << "  -v, --verbose       Verbose output\n";
            std::cout << '\n';
            std::cout << "Examples:\n";
            std::cout << "  omninet-firewall list\n";
            std::cout << "  omninet-firewall block 192.168.1.100\n";
            std::cout << "  omninet-firewall block 192.168.1.100 --duration 1h\n";
            std::cout << "  omninet-firewall unblock 192.168.1.100\n";
            std::cout << "  omninet-firewall unblock --all\n";
            std::cout << "  omninet-firewall logs\n";
            exit(0);
        }
        else if (arg == "list" || arg == "blocked" || 
                 arg == "block" || arg == "unblock" ||
                 arg == "logs" || arg == "log" ||
                 arg == "rules" || arg == "whitelist") {
            opts.action = arg;
        }
        else if (arg == "-d" || arg == "--duration") {
            if (i + 1 < argc) {
                opts.duration = argv[++i];
            }
        }
        else if (arg == "-s" || arg == "--since") {
            if (i + 1 < argc) {
                opts.since = argv[++i];
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] != '-') {
            opts.target = arg;
        }
    }
    
    return opts;
}

// ============================================================================
// Duration Parsing
// ============================================================================

std::chrono::minutes parseDuration(const std::string& str) {
    if (str.empty()) {
        return std::chrono::minutes(30);
    }
    
    size_t i = 0;
    while (i < str.length() && std::isdigit(str[i])) {
        i++;
    }
    
    int value = 1;
    try {
        value = std::stoi(str.substr(0, i));
    } catch (...) {
        return std::chrono::minutes(30);
    }
    
    std::string unit = str.substr(i);
    
    if (unit == "s" || unit == "sec" || unit == "seconds") {
        return std::chrono::minutes(value / 60 > 0 ? value / 60 : 1);
    }
    else if (unit == "m" || unit == "min" || unit == "minutes" || unit.empty()) {
        return std::chrono::minutes(value);
    }
    else if (unit == "h" || unit == "hr" || unit == "hours") {
        return std::chrono::hours(value);
    }
    else if (unit == "d" || unit == "day" || unit == "days") {
        return std::chrono::hours(value * 24);
    }
    else {
        return std::chrono::minutes(30);
    }
}
