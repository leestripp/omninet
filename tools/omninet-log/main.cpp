// omninet-log - Log Viewer & Analyzer for OmniNet
// View, filter, search, and analyze OmniNet logs

#include <iostream>
#include <thread>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <chrono>
#include <ctime>
#include <fstream>
#include <signal.h>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"

using namespace OmniNet::Tools;

namespace fs = std::filesystem;

constexpr const char* TOOL_NAME = "omninet-log";

// ============================================================================
// Global state
// ============================================================================

volatile sig_atomic_t g_running = true;

void signalHandler(int) {
    g_running = false;
}

// ============================================================================
// Configuration
// ============================================================================

struct LogOptions {
    bool follow = false;
    bool tail = false;
    int tailLines = 50;
    std::string level;
    std::string grep;
    std::string since;
    bool stats = false;
    std::string file;
};

LogOptions parseArgs(int argc, char* argv[]);

// ============================================================================
// Log Entry (parsed from line)
// ============================================================================

struct LogEntry {
    std::string level;
    std::string timestamp;
    std::string message;
    std::string raw;
};

LogEntry parseLogLine(const std::string& line);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    // Determine log file path
    fs::path logPath;
    if (!opts.file.empty()) {
        logPath = opts.file;
    } else {
        logPath = Paths::logFile();
    }
    
    // Check if file exists
    if (!fs::exists(logPath)) {
        std::cerr << "Error: Log file not found: " << logPath.string() << '\n';
        return 1;
    }
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    // Open file
    std::ifstream file(logPath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open log file: " << logPath.string() << '\n';
        return 1;
    }
    
    // Statistics
    size_t totalLines = 0;
    size_t debugCount = 0, infoCount = 0, warnCount = 0, errorCount = 0, criticalCount = 0;
    
    // Pattern for grep
    std::regex grepPattern;
    bool hasGrep = false;
    if (!opts.grep.empty()) {
        try {
            grepPattern = std::regex(opts.grep, std::regex::icase);
            hasGrep = true;
        } catch (const std::regex_error& e) {
            std::cerr << "Error: Invalid grep pattern: " << e.what() << '\n';
            return 1;
        }
    }
    
    // Follow mode
    if (opts.follow) {
        // Set up signal handler
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Seek to end of file
        file.seekg(0, std::ios::end);
        size_t lastPos = file.tellg();
        
        std::cout << "Following log file (Ctrl+C to stop)...\n\n";
        
        while (g_running) {
            file.seekg(lastPos);
            
            std::string line;
            while (std::getline(file, line) && g_running) {
                // Parse and display line
                auto entry = parseLogLine(line);
                
                // Apply filters
                if (!opts.level.empty() && entry.level != opts.level) {
                    continue;
                }
                if (hasGrep && !std::regex_search(entry.message, grepPattern)) {
                    continue;
                }
                
                // Display
                Text style = Style::NORMAL;
                if (entry.level == "ERROR") style = Style::ERROR;
                else if (entry.level == "WARN") style = Style::WARNING;
                else if (entry.level == "INFO") style = Style::INFO;
                else if (entry.level == "DEBUG") style = Style::DEBUG;
                else if (entry.level == "CRITICAL") style = {Color::BRIGHT_RED, true, false};
                
                out.text(entry.raw, style);
                
                lastPos = file.tellg();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        std::cout << "\n\nStopped following log.\n";
        return 0;
    }
    
    // Read lines (tail or all)
    std::vector<LogEntry> entries;
    std::vector<std::string> rawLines;
    
    if (opts.tail) {
        // Read all, keep last N lines
        std::string line;
        while (std::getline(file, line)) {
            rawLines.push_back(line);
            if (rawLines.size() > static_cast<size_t>(opts.tailLines)) {
                rawLines.erase(rawLines.begin());
            }
        }
    } else {
        // Read all
        std::string line;
        while (std::getline(file, line)) {
            rawLines.push_back(line);
        }
    }
    
    // Parse and filter
    for (const auto& raw : rawLines) {
        auto entry = parseLogLine(raw);
        entries.push_back(entry);
        totalLines++;
        
        if (entry.level == "DEBUG") debugCount++;
        else if (entry.level == "INFO") infoCount++;
        else if (entry.level == "WARN") warnCount++;
        else if (entry.level == "ERROR") errorCount++;
        else if (entry.level == "CRITICAL") criticalCount++;
        
        // Apply filters
        if (!opts.level.empty() && entry.level != opts.level) {
            continue;
        }
        if (hasGrep && !std::regex_search(entry.message, grepPattern)) {
            continue;
        }
        
        // Display
        Text style = Style::NORMAL;
        if (entry.level == "ERROR") style = Style::ERROR;
        else if (entry.level == "WARN") style = Style::WARNING;
        else if (entry.level == "INFO") style = Style::INFO;
        else if (entry.level == "DEBUG") style = Style::DEBUG;
        else if (entry.level == "CRITICAL") style = {Color::BRIGHT_RED, true, false};
        
        std::cout << raw << '\n';
    }
    
    // Statistics output
    if (opts.stats) {
        std::cout << '\n';
        out.header("Log Statistics");
        out.kv("Total Lines", totalLines);
        out.kv("DEBUG", debugCount);
        out.kv("INFO", infoCount);
        out.kv("WARN", warnCount);
        out.kv("ERROR", errorCount);
        out.kv("CRITICAL", criticalCount);
    }
    
    return 0;
}

// ============================================================================
// Argument Parsing
// ============================================================================

LogOptions parseArgs(int argc, char* argv[]) {
    LogOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << TOOL_NAME << " - OmniNet Log Viewer\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " [OPTIONS] [file]\n";
            std::cout << '\n';
            std::cout << "View and analyze OmniNet logs.\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help         Show this help\n";
            std::cout << "  -V, --version      Show version information\n";
            std::cout << "  -f, --follow       Follow log file (like tail -f)\n";
            std::cout << "  -t, --tail         Show last N lines (default: 50)\n";
            std::cout << "  -n, --lines        Number of lines for tail (default: 50)\n";
            std::cout << "  -l, --level        Filter by log level (DEBUG, INFO, WARN, ERROR, CRITICAL)\n";
            std::cout << "  -g, --grep         Search pattern (grep-style)\n";
            std::cout << "  -s, --since        Show entries since timestamp\n";
            std::cout << "  -S, --stats        Show statistics\n";
            std::cout << "  -j, --json         Output as JSON\n";
            std::cout << "  -v, --verbose      Verbose output\n";
            std::cout << '\n';
            std::cout << "Examples:\n";
            std::cout << "  " << TOOL_NAME << "\n";
            std::cout << "  " << TOOL_NAME << " --follow\n";
            std::cout << "  " << TOOL_NAME << " --tail\n";
            std::cout << "  " << TOOL_NAME << " --level ERROR\n";
            std::cout << "  " << TOOL_NAME << " --grep 'failed login'\n";
            std::cout << "  " << TOOL_NAME << " --stats\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "-f" || arg == "--follow") {
            opts.follow = true;
        }
        else if (arg == "-t" || arg == "--tail") {
            opts.tail = true;
        }
        else if (arg == "-n" || arg == "--lines") {
            if (i + 1 < argc) {
                opts.tailLines = std::stoi(argv[++i]);
                opts.tail = true;
            }
        }
        else if (arg == "-l" || arg == "--level") {
            if (i + 1 < argc) {
                opts.level = argv[++i];
            }
        }
        else if (arg == "-g" || arg == "--grep") {
            if (i + 1 < argc) {
                opts.grep = argv[++i];
            }
        }
        else if (arg == "-s" || arg == "--since") {
            if (i + 1 < argc) {
                opts.since = argv[++i];
            }
        }
        else if (arg == "-S" || arg == "--stats") {
            opts.stats = true;
        }
        else if (arg == "-j" || arg == "--json") {
            opts.stats = true; // JSON implies stats
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.stats = true;
        }
        else if (arg[0] != '-') {
            opts.file = arg;
        }
    }
    
    return opts;
}

// ============================================================================
// Log Parsing
// ============================================================================

LogEntry parseLogLine(const std::string& line) {
    LogEntry entry;
    entry.raw = line;
    
    // Expected format: [LEVEL] [timestamp] [thread] message
    // Example: [INFO] [2024-01-15 10:30:45] [1234] Server started
    
    static std::regex logRegex(R"(\[(\w+)\]\s*\[([^\]]+)\]\s*(?:\[(\d+)\])?\s*(.*))");
    std::smatch match;
    
    if (std::regex_match(line, match, logRegex)) {
        entry.level = match[1];
        entry.timestamp = match[2];
        entry.message = match[4];
    } else {
        // Try simpler format
        if (line.find("[DEBUG]") != std::string::npos) entry.level = "DEBUG";
        else if (line.find("[INFO]") != std::string::npos) entry.level = "INFO";
        else if (line.find("[WARN]") != std::string::npos) entry.level = "WARN";
        else if (line.find("[ERROR]") != std::string::npos) entry.level = "ERROR";
        else if (line.find("[CRITICAL]") != std::string::npos) entry.level = "CRITICAL";
        
        entry.message = line;
    }
    
    return entry;
}
