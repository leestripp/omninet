// OmniNet - Log Reader Utility
#ifndef OMNINET_LOG_READER_H
#define OMNINET_LOG_READER_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>
#include <filesystem>
#include <atomic>
#include <thread>

#include "Logger.h"

namespace OmniNet {

// ============================================================================
// Log Entry
// ============================================================================

struct LogEntry {
    LogLevel level;           // Parsed log level
    std::string timestamp;    // Original timestamp string
    std::string threadId;     // Thread ID
    std::string message;       // Log message content
    std::string rawLine;       // Original unparsed line
    
    // For JSON serialization
    std::string toJson() const;
};

// ============================================================================
// Log Reader
// ============================================================================

class LogReader {
public:
    // Construct with path to log file
    explicit LogReader(const std::string& logPath);
    
    // Construct with path to log file (std::filesystem::path)
    explicit LogReader(const std::filesystem::path& logPath);
    
    // Virtual destructor for inheritance
    virtual ~LogReader() = default;
    
    // --- Reading Operations ---
    
    // Read last N lines from log file
    std::vector<LogEntry> readLast(size_t count);
    
    // Read lines from offset (for pagination)
    std::vector<LogEntry> readFrom(size_t offset, size_t count);
    
    // Read lines within a time range
    std::vector<LogEntry> readRange(
        const std::chrono::system_clock::time_point& from,
        const std::chrono::system_clock::time_point& to
    );
    
    // Read all lines (careful with large files!)
    std::vector<LogEntry> readAll();
    
    // --- Filtering ---
    
    // Set minimum log level filter
    void setMinLevel(LogLevel level);
    LogLevel minLevel() const;
    
    // Set pattern filter (substring match in message)
    void setPattern(const std::string& pattern);
    const std::optional<std::string>& pattern() const;
    
    // --- Tail Mode ---
    
    // Start tailing - calls callback for each new line
    // Returns stop function
    using StopCallback = std::function<void()>;
    StopCallback tail(
        std::function<void(const LogEntry&)> callback,
        std::chrono::milliseconds pollInterval = std::chrono::milliseconds(500)
    );
    
    // Stop tailing
    void stopTail();
    
    // Is currently tailing?
    bool isTailing() const;
    
    // --- File Info ---
    
    // Current file size
    size_t fileSize() const;
    
    // Last modified time
    std::optional<std::chrono::system_clock::time_point> lastModified() const;
    
    // Check if log file exists
    bool exists() const;
    
    // Number of lines in file (expensive - scans entire file)
    size_t lineCount() const;
    
    // Get list of rotated log files
    std::vector<std::filesystem::path> rotatedFiles() const;
    
    // --- Utility ---
    
    // Parse a single log line into LogEntry
    static std::optional<LogEntry> parseLine(const std::string& line);
    
    // Parse log level from string
    static LogLevel parseLevel(const std::string& levelStr);
    
    // Convert log level to string
    static std::string levelToString(LogLevel level);

private:
    std::filesystem::path m_logPath;
    std::optional<LogLevel> m_minLevel;
    std::optional<std::string> m_pattern;
    
    // For tail mode
    std::atomic<bool> m_tailing{false};
    std::thread m_tailThread;
    size_t m_lastReadPosition = 0;
    
    // Internal helpers
    std::vector<LogEntry> readLines(size_t startLine, size_t count);
    bool matchesFilter(const LogEntry& entry) const;
};

} // namespace OmniNet

#endif // OMNINET_LOG_READER_H
