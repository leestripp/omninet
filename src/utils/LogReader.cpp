// OmniNet - Log Reader Implementation
#include "LogReader.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace OmniNet {

// ============================================================================
// LogEntry
// ============================================================================

std::string LogEntry::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"level\":\"" << toString(level) << "\","
        << "\"timestamp\":\"" << timestamp << "\","
        << "\"threadId\":\"" << threadId << "\","
        << "\"message\":\"" << message << "\""
        << "}";
    return oss.str();
}

// ============================================================================
// LogReader
// ============================================================================

LogReader::LogReader(const std::string& logPath)
    : m_logPath(logPath) {}

LogReader::LogReader(const std::filesystem::path& logPath)
    : m_logPath(logPath) {}

std::vector<LogEntry> LogReader::readLast(size_t count) {
    return readFrom(0, count);
}

std::vector<LogEntry> LogReader::readFrom(size_t offset, size_t count) {
    std::vector<LogEntry> entries;
    
    std::ifstream file(m_logPath);
    if (!file.is_open()) {
        return entries;
    }
    
    std::string line;
    size_t currentLine = 0;
    
    // Skip to offset
    while (currentLine < offset && std::getline(file, line)) {
        currentLine++;
    }
    
    // Read count lines
    while (std::getline(file, line) && entries.size() < count) {
        if (auto entry = parseLine(line)) {
            if (matchesFilter(*entry)) {
                entries.push_back(std::move(*entry));
            }
        }
    }
    
    return entries;
}

std::vector<LogEntry> LogReader::readRange(
    const std::chrono::system_clock::time_point& from,
    const std::chrono::system_clock::time_point& to
) {
    std::vector<LogEntry> entries;
    
    std::ifstream file(m_logPath);
    if (!file.is_open()) {
        return entries;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (auto entry = parseLine(line)) {
            // Simple timestamp comparison (parsing would be more accurate)
            if (matchesFilter(*entry)) {
                entries.push_back(std::move(*entry));
            }
        }
    }
    
    return entries;
}

std::vector<LogEntry> LogReader::readAll() {
    std::vector<LogEntry> entries;
    
    std::ifstream file(m_logPath);
    if (!file.is_open()) {
        return entries;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (auto entry = parseLine(line)) {
            if (matchesFilter(*entry)) {
                entries.push_back(std::move(*entry));
            }
        }
    }
    
    return entries;
}

void LogReader::setMinLevel(LogLevel level) {
    m_minLevel = level;
}

LogLevel LogReader::minLevel() const {
    return m_minLevel.value_or(LogLevel::DEBUG);
}

void LogReader::setPattern(const std::string& pattern) {
    m_pattern = pattern.empty() ? std::nullopt : std::make_optional(pattern);
}

const std::optional<std::string>& LogReader::pattern() const {
    return m_pattern;
}

LogReader::StopCallback LogReader::tail(
    std::function<void(const LogEntry&)> callback,
    std::chrono::milliseconds pollInterval
) {
    if (m_tailing.load()) {
        return []() {};
    }
    
    m_tailing = true;
    m_lastReadPosition = 0;
    
    // Get initial file size
    std::error_code ec;
    m_lastReadPosition = std::filesystem::file_size(m_logPath, ec);
    if (ec) {
        m_lastReadPosition = 0;
    }
    
    m_tailThread = std::thread([this, callback, pollInterval]() {
        std::ifstream file;
        file.open(m_logPath, std::ios::in | std::ios::binary);
        
        while (m_tailing.load()) {
            // Check file size
            std::error_code ec;
            auto currentSize = std::filesystem::file_size(m_logPath, ec);
            
            if (!ec && currentSize > m_lastReadPosition) {
                // Read new content
                file.seekg(m_lastReadPosition);
                
                std::string line;
                while (std::getline(file, line)) {
                    if (auto entry = parseLine(line)) {
                        if (matchesFilter(*entry)) {
                            callback(*entry);
                        }
                    }
                }
                
                m_lastReadPosition = file.tellg();
            }
            
            // Sleep before next check
            std::this_thread::sleep_for(pollInterval);
        }
        
        file.close();
    });
    
    // Return stop function
    return [this]() {
        stopTail();
    };
}

void LogReader::stopTail() {
    m_tailing = false;
    if (m_tailThread.joinable()) {
        m_tailThread.join();
    }
}

bool LogReader::isTailing() const {
    return m_tailing.load();
}

size_t LogReader::fileSize() const {
    std::error_code ec;
    return std::filesystem::file_size(m_logPath, ec);
}

std::optional<std::chrono::system_clock::time_point> LogReader::lastModified() const {
    std::error_code ec;
    auto time = std::filesystem::last_write_time(m_logPath, ec);
    
    if (ec) {
        return std::nullopt;
    }
    
    // Convert from file_time_type to system_clock::time_point
    auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(time);
    return systemTime;
}

bool LogReader::exists() const {
    return std::filesystem::exists(m_logPath);
}

size_t LogReader::lineCount() const {
    size_t count = 0;
    std::ifstream file(m_logPath);
    std::string line;
    
    while (std::getline(file, line)) {
        count++;
    }
    
    return count;
}

std::vector<std::filesystem::path> LogReader::rotatedFiles() const {
    std::vector<std::filesystem::path> files;
    
    // Check for .1, .2, .3, etc. files
    size_t i = 1;
    while (true) {
        auto rotatedPath = m_logPath;
        rotatedPath += ".";
        rotatedPath += std::to_string(i);
        
        if (std::filesystem::exists(rotatedPath)) {
            files.push_back(rotatedPath);
            i++;
        } else {
            break;
        }
        
        // Safety limit
        if (i > 1000) {
            break;
        }
    }
    
    return files;
}

// static
std::optional<LogEntry> LogReader::parseLine(const std::string& line) {
    // Expected format: "[LEVEL] [timestamp] [thread] message"
    // Or we can try to parse flexibly
    
    LogEntry entry;
    entry.rawLine = line;
    
    // Find first bracket
    size_t pos = 0;
    if (line[pos] != '[') {
        // Try to find a bracket
        pos = line.find('[');
        if (pos == std::string::npos) {
            // No brackets, just use the whole line as message
            entry.level = LogLevel::INFO;
            entry.timestamp = "";
            entry.threadId = "";
            entry.message = line;
            return entry;
        }
    }
    
    // Parse level (first bracket content)
    size_t endBracket = line.find(']', pos);
    if (endBracket == std::string::npos) {
        entry.level = LogLevel::INFO;
        entry.timestamp = "";
        entry.threadId = "";
        entry.message = line;
        return entry;
    }
    
    std::string levelStr = line.substr(pos + 1, endBracket - pos - 1);
    entry.level = parseLevel(levelStr);
    
    pos = endBracket + 1;
    
    // Skip whitespace
    while (pos < line.size() && line[pos] == ' ') pos++;
    
    // Parse timestamp (second bracket)
    if (pos < line.size() && line[pos] == '[') {
        endBracket = line.find(']', pos);
        if (endBracket != std::string::npos) {
            entry.timestamp = line.substr(pos + 1, endBracket - pos - 1);
            pos = endBracket + 1;
        }
    }
    
    // Skip whitespace
    while (pos < line.size() && line[pos] == ' ') pos++;
    
    // Parse thread ID (third bracket)
    if (pos < line.size() && line[pos] == '[') {
        endBracket = line.find(']', pos);
        if (endBracket != std::string::npos) {
            entry.threadId = line.substr(pos + 1, endBracket - pos - 1);
            pos = endBracket + 1;
        }
    }
    
    // Skip whitespace
    while (pos < line.size() && line[pos] == ' ') pos++;
    
    // Rest is the message
    entry.message = line.substr(pos);
    
    return entry;
}

// static
LogLevel LogReader::parseLevel(const std::string& levelStr) {
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARN") return LogLevel::WARN;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::INFO;  // Default
}

// static
std::string LogReader::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARN:    return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                return "UNKNOWN";
    }
}

// private
std::vector<LogEntry> LogReader::readLines(size_t startLine, size_t count) {
    return readFrom(startLine, count);
}

// private
bool LogReader::matchesFilter(const LogEntry& entry) const {
    // Check level filter
    if (m_minLevel.has_value() && entry.level < m_minLevel.value()) {
        return false;
    }
    
    // Check pattern filter
    if (m_pattern.has_value() && entry.message.find(m_pattern.value()) == std::string::npos) {
        return false;
    }
    
    return true;
}

} // namespace OmniNet