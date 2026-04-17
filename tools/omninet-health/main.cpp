// omninet-health - Health Check Tool for OmniNet Servers
// Checks if server endpoints are responding correctly

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/http.h"

using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-health";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string url;
    std::vector<std::string> endpoints;
    std::string expectedBody;
    int timeout = 10;
    bool json = false;
    bool verbose = false;
    bool checkAll = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    // Check required arguments
    if (opts.url.empty()) {
        std::cerr << "Error: URL is required\n";
        std::cerr << "Usage: omninet-health <server-url> [endpoints...]\n";
        std::cerr << "       omninet-health --help\n";
        return 1;
    }
    
    // Normalize URL
    if (!opts.url.empty() && opts.url.back() == '/') {
        opts.url.pop_back();
    }
    
    // Set up health checker
    HealthChecker checker(opts.url);
    checker.timeout(std::chrono::seconds(opts.timeout));
    
    // Default endpoints if --all specified
    if (opts.checkAll) {
        opts.endpoints = HealthChecker::defaultEndpoints();
    }
    
    // Add built-in admin endpoint if no endpoints specified
    if (opts.endpoints.empty()) {
        opts.endpoints.push_back("/omninet/health");
    }
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    // Run health checks
    bool allHealthy = true;
    int checkedCount = 0;
    
    for (const auto& endpoint : opts.endpoints) {
        checkedCount++;
        
        HealthCheckResult result;
        if (opts.expectedBody.empty()) {
            result = checker.check(endpoint);
        } else {
            result = checker.check(endpoint, opts.expectedBody);
        }
        
        // Output result
        if (result.healthy) {
            if (!opts.json) {
                out.text("[OK] ");
            }
            out.text(endpoint + " - " + std::to_string(result.statusCode) + 
                     " (" + std::to_string(result.responseTime.count()) + "ms)", 
                     Style::SUCCESS);
            if (opts.verbose && !opts.json) {
                std::cout << '\n';
            }
        } else {
            allHealthy = false;
            if (!opts.json) {
                out.text("[FAIL] ");
            }
            out.text(endpoint, Style::ERROR);
            if (!opts.json) {
                std::cout << " - ";
                if (!result.error.empty()) {
                    out.text(result.error, Style::ERROR);
                } else {
                    out.text("HTTP " + std::to_string(result.statusCode), Style::ERROR);
                }
                std::cout << " (" << result.responseTime.count() << "ms)\n";
            }
        }
        
        // Verbose output
        if (opts.verbose && result.healthy) {
            if (!result.responseBody.empty() && result.responseBody.length() < 200) {
                std::cout << "    Response: " << result.responseBody << '\n';
            }
        }
    }
    
    // Summary
    if (!opts.json) {
        std::cout << '\n';
        
        if (allHealthy) {
            out.success("All checks passed (" + std::to_string(checkedCount) + "/" + 
                       std::to_string(checkedCount) + ")");
        } else {
            int failedCount = checkedCount - (allHealthy ? checkedCount : 0);
            out.warning("Some checks failed (" + 
                       std::to_string(checkedCount - failedCount) + "/" + 
                       std::to_string(checkedCount) + " passed)");
        }
    } else {
        // JSON output
        std::cout << "{\n";
        std::cout << "  \"target\": \"" << opts.url << "\",\n";
        std::cout << "  \"healthy\": " << (allHealthy ? "true" : "false") << ",\n";
        std::cout << "  \"checked\": " << checkedCount << ",\n";
        std::cout << "  \"passed\": " << (allHealthy ? checkedCount : 0) << "\n";
        std::cout << "}\n";
    }
    
    return allHealthy ? 0 : 1;
}

// ============================================================================
// Argument Parsing
// ============================================================================

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << TOOL_NAME << " - OmniNet Health Checker\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " <server-url> [endpoints...]\n";
            std::cout << '\n';
            std::cout << "Health check tool for OmniNet servers.\n";
            std::cout << '\n';
            std::cout << "Arguments:\n";
            std::cout << "  server-url          Base URL of the server (e.g., http://localhost:8080)\n";
            std::cout << "  endpoints           Additional endpoints to check (optional)\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help          Show this help message\n";
            std::cout << "  -V, --version      Show version information\n";
            std::cout << "  -a, --all           Check all default endpoints\n";
            std::cout << "  -e, --expected      Expected response body substring\n";
            std::cout << "  -t, --timeout       Request timeout in seconds (default: 10)\n";
            std::cout << "  -j, --json          Output as JSON\n";
            std::cout << "  -v, --verbose       Show verbose output\n";
            std::cout << '\n';
            std::cout << "Examples:\n";
            std::cout << "  " << TOOL_NAME << " http://localhost:8080\n";
            std::cout << "  " << TOOL_NAME << " http://localhost:8080 /omninet/health\n";
            std::cout << "  " << TOOL_NAME << " --all http://localhost:8080\n";
            std::cout << "  " << TOOL_NAME << " -e '{\"status\":\"ok\"}' http://localhost:8080\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "-a" || arg == "--all") {
            opts.checkAll = true;
        }
        else if (arg == "-e" || arg == "--expected") {
            if (i + 1 < argc) {
                opts.expectedBody = argv[++i];
            }
        }
        else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                opts.timeout = std::stoi(argv[++i]);
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg.empty() || arg[0] != '-') {
            // URL or endpoint
            if (opts.url.empty()) {
                opts.url = arg;
            } else {
                opts.endpoints.push_back(arg);
            }
        }
    }
    
    return opts;
}
