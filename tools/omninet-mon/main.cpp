// omninet-mon - Real-time Server Monitor
// Watch server statistics and health in real-time

#include <iostream>
#include <thread>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/http.h"

using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-mon";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string url = "http://localhost:8080";
    int interval = 2;              // seconds
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Stats
// ============================================================================

struct ServerStats {
    std::chrono::system_clock::time_point timestamp;
    int64_t requestsTotal = 0;
    int64_t requestsActive = 0;
    int64_t requestsSuccess = 0;
    int64_t requestsError = 0;
    int64_t bytesReceived = 0;
    int64_t bytesSent = 0;
    int64_t connectionsTotal = 0;
    int64_t connectionsActive = 0;
    int responseTime = 0;
    bool healthy = false;
};

ServerStats fetchStats(const std::string& url, int timeoutSeconds);

// ============================================================================
// Display
// ============================================================================

void displayStats(const ServerStats& current, const ServerStats& previous, 
                  bool useColors, bool verbose);

std::string formatBytes(int64_t bytes);
std::string formatNumber(int64_t num);

// ============================================================================
// Global
// ============================================================================

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    
    // Set up signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "OmniNet Server Monitor\n";
    std::cout << "======================\n";
    std::cout << "Target: " << opts.url << '\n';
    std::cout << "Interval: " << opts.interval << "s\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    
    ServerStats previous;
    
    while (g_running) {
        auto current = fetchStats(opts.url, opts.interval);
        
        displayStats(current, previous, useColors, opts.verbose);
        
        previous = current;
        
        if (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(opts.interval));
        }
    }
    
    std::cout << "\n\nStopped.\n";
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
            std::cout << TOOL_NAME << " - OmniNet Server Monitor\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " [options]\n";
            std::cout << '\n';
            std::cout << "Real-time server monitoring for OmniNet.\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help          Show this help\n";
            std::cout << "  -V, --version       Show version information\n";
            std::cout << "  -u, --url           Server URL (default: http://localhost:8080)\n";
            std::cout << "  -i, --interval      Update interval in seconds (default: 2)\n";
            std::cout << "  -j, --json          Output as JSON\n";
            std::cout << "  -v, --verbose       Verbose output\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "-u" || arg == "--url") {
            if (i + 1 < argc) {
                opts.url = argv[++i];
            }
        }
        else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                opts.interval = std::stoi(argv[++i]);
                if (opts.interval < 1) opts.interval = 1;
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
    }
    
    return opts;
}

// ============================================================================
// Stats Fetching
// ============================================================================

ServerStats fetchStats(const std::string& baseUrl, int timeoutSeconds) {
    ServerStats stats;
    stats.timestamp = std::chrono::system_clock::now();
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Try to get stats from dashboard API
    HealthChecker checker(baseUrl);
    checker.timeout(std::chrono::seconds(timeoutSeconds));
    
    auto health = checker.check("/api/stats");
    
    stats.responseTime = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count());
    
    stats.healthy = health.healthy;
    
    if (health.healthy && !health.responseBody.empty()) {
        // Parse JSON response if available
        try {
            // Simple JSON parsing (would use nlohmann::json in real implementation)
            // For now, just mark as healthy if we got a response
        } catch (...) {
            // Ignore parse errors
        }
    }
    
    return stats;
}

// ============================================================================
// Display
// ============================================================================

void displayStats(const ServerStats& current, const ServerStats& previous,
                 bool useColors, bool verbose) {
    // Clear line
    std::cout << "\033[2J\033[1;1H";
    
    // Header
    auto ts = std::chrono::system_clock::to_time_t(current.timestamp);
    std::tm tm = *std::localtime(&ts);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    
    if (useColors) {
        std::cout << "\033[1m" << buf << "\033[0m";
        if (current.healthy) {
            std::cout << " \033[32m● HEALTHY\033[0m";
        } else {
            std::cout << " \033[31m● UNHEALTHY\033[0m";
        }
    } else {
        std::cout << buf;
        if (current.healthy) {
            std::cout << " [HEALTHY]";
        } else {
            std::cout << " [UNHEALTHY]";
        }
    }
    
    std::cout << " Response: " << current.responseTime << "ms\n";
    std::cout << std::string(50, '-') << '\n';
    
    // Stats
    std::cout << "Requests:     " << formatNumber(current.requestsTotal) << " total\n";
    std::cout << "Active:       " << formatNumber(current.requestsActive) << '\n';
    std::cout << "Success:      " << formatNumber(current.requestsSuccess) << '\n';
    std::cout << "Errors:       " << formatNumber(current.requestsError) << '\n';
    std::cout << '\n';
    std::cout << "Bytes In:     " << formatBytes(current.bytesReceived) << '\n';
    std::cout << "Bytes Out:    " << formatBytes(current.bytesSent) << '\n';
    std::cout << '\n';
    std::cout << "Connections:  " << formatNumber(current.connectionsActive) << " active\n";
    std::cout << "Total:        " << formatNumber(current.connectionsTotal) << '\n';
    
    if (verbose) {
        std::cout << '\n';
        std::cout << "Delta (since last):\n";
        int64_t deltaReq = current.requestsTotal - previous.requestsTotal;
        int64_t deltaConn = current.connectionsActive - previous.connectionsActive;
        std::cout << "  Requests/sec:      " << deltaReq << '\n';
        std::cout << "  Connection change: " << (deltaConn >= 0 ? "+" : "") << deltaConn << '\n';
    }
    
    std::cout << std::flush;
}

std::string formatBytes(int64_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    } else {
        return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
    }
}

std::string formatNumber(int64_t num) {
    if (num < 1000) {
        return std::to_string(num);
    } else if (num < 1000000) {
        return std::to_string(num / 1000) + "K";
    } else {
        return std::to_string(num / 1000000) + "M";
    }
}
