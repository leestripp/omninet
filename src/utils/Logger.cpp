// OmniNet - Logger Implementation
#include "Logger.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

// Forward declaration to avoid circular dependency
namespace OmniNet {
struct LoggingConfig;
}

// Need to include Config to get LoggingConfig definition
#include "config/Config.h"

namespace OmniNet {

namespace {
    // Thread ID as string
    std::string getThreadId() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }
    
    // Current timestamp as string
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;
        
        // v0.6.0 fix (bug #7): use localtime_r instead of localtime.
        // std::localtime returns a pointer to a shared static buffer,
        // which is a data race when called from multiple threads
        // (Logger can be called from worker threads; AsyncLogger by
        // definition is multi-threaded).
        std::tm tm_buf{};
        ::localtime_r(&time, &tm_buf);
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
}

// ============================================================================
// Logger Singleton
// ============================================================================

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    m_queue.reserve(1024);
}

Logger::~Logger() {
    flush();
}

void Logger::setLevel(LogLevel level) {
    m_level = level;
}

LogLevel Logger::level() const {
    return m_level.load();
}

void Logger::setLogFile(const std::string& path) {
    m_logFile = path.empty() ? std::nullopt : std::make_optional(path);
}

const std::optional<std::string>& Logger::logFile() const {
    return m_logFile;
}

void Logger::setAsync(bool async) {
    m_async = async;
}

bool Logger::isAsync() const {
    return m_async;
}

void Logger::setHandler(Handler handler) {
    m_handler = std::move(handler);
}

void Logger::setMaxFileSize(size_t bytes) {
    m_maxFileSize = bytes;
}

void Logger::setMaxRotatedFiles(size_t count) {
    m_maxRotatedFiles = count;
}

void Logger::flush() {
    std::vector<AsyncLogEntry> entries;
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        entries = std::move(m_queue);
        m_queue.clear();
    }
    
    for (const auto& entry : entries) {
        writeLog(entry.level, entry.message);
    }
}

std::string Logger::format(LogLevel level, const std::string& message) {
    std::ostringstream oss;
    oss << '[' << toString(level) << "] "
        << '[' << getTimestamp() << "] "
        << '[' << getThreadId() << "] "
        << message;
    return oss.str();
}

std::string Logger::format(LogLevel level, std::string_view message) {
    return format(level, std::string(message));
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_level.load()) {
        return;
    }
    
    if (m_async) {
        enqueueLog({level, message, std::chrono::steady_clock::now()});
    } else {
        writeLog(level, format(level, message));
    }
}

void Logger::log(LogLevel level, std::string_view message) {
    log(level, std::string(message));
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::debug(std::string_view message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::info(std::string_view message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::warn(std::string_view message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::error(std::string_view message) {
    log(LogLevel::ERROR, message);
}

void Logger::critical(const std::string& message) {
    log(LogLevel::CRITICAL, message);
}

void Logger::critical(std::string_view message) {
    log(LogLevel::CRITICAL, message);
}

void Logger::writeLog(LogLevel level, const std::string& formatted) {
    // Write to custom handler if set
    if (m_handler) {
        try {
            m_handler(level, formatted);
        } catch (...) {
            // Ignore handler errors
        }
    }
    
    // Write to console
    if (level >= LogLevel::ERROR) {
        std::fprintf(stderr, "%s\n", formatted.c_str());
    } else {
        std::printf("%s\n", formatted.c_str());
    }
    
    // Write to file if configured
    if (m_logFile) {
        std::FILE* f = std::fopen(m_logFile->c_str(), "a");
        if (f) {
            std::fprintf(f, "%s\n", formatted.c_str());
            std::fclose(f);
        }
        
        // Check if rotation is needed
        performRotation();
    }
}

void Logger::enqueueLog(AsyncLogEntry entry) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push_back(std::move(entry));
    }
}

void Logger::rotate() {
    if (!m_logFile) {
        return;
    }
    
    const std::string& logPath = *m_logFile;
    size_t maxFiles = m_maxRotatedFiles.load();
    
    // If maxFiles is 0, don't rotate (disabled)
    if (maxFiles == 0) {
        return;
    }
    
    namespace fs = std::filesystem;
    
    try {
        // Close any existing file handles by flushing
        flush();
        
        // Check if log file exists
        if (!fs::exists(logPath)) {
            return;
        }
        
        // Delete the oldest file if it exists (highest number)
        std::string oldestPath = logPath + "." + std::to_string(maxFiles);
        if (fs::exists(oldestPath)) {
            fs::remove(oldestPath);
        }
        
        // Rename existing rotated files (n -> n+1)
        for (size_t i = maxFiles - 1; i > 0; --i) {
            std::string srcPath = logPath + "." + std::to_string(i);
            std::string dstPath = logPath + "." + std::to_string(i + 1);
            
            if (fs::exists(srcPath)) {
                fs::rename(srcPath, dstPath);
            }
        }
        
        // Rename current log file to .1
        std::string rotatedPath = logPath + ".1";
        if (fs::exists(rotatedPath)) {
            fs::remove(rotatedPath);
        }
        fs::rename(logPath, rotatedPath);
        
        // Log that rotation occurred
        std::string msg = "Log rotated: " + logPath + " -> " + rotatedPath;
        std::fprintf(stderr, "%s\n", format(LogLevel::INFO, msg).c_str());
        
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Log rotation failed: %s\n", e.what());
    }
}

void Logger::performRotation() {
    if (!m_logFile || m_maxFileSize == 0) {
        return;
    }
    
    // Check file size (platform-dependent)
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExA(m_logFile->c_str(), GetFileExInfoStandard, &attrs)) {
        if (attrs.nFileSizeHigh || attrs.nFileSizeLow > m_maxFileSize) {
            rotate();
        }
    }
#else
    struct stat st;
    if (stat(m_logFile->c_str(), &st) == 0) {
        if (static_cast<size_t>(st.st_size) > m_maxFileSize) {
            rotate();
        }
    }
#endif
}

void Logger::initFromConfig(const LoggingConfig& config) {
    // Set log level
    if (config.level == "DEBUG") {
        setLevel(LogLevel::DEBUG);
    } else if (config.level == "INFO") {
        setLevel(LogLevel::INFO);
    } else if (config.level == "WARN") {
        setLevel(LogLevel::WARN);
    } else if (config.level == "ERROR") {
        setLevel(LogLevel::ERROR);
    } else if (config.level == "CRITICAL") {
        setLevel(LogLevel::CRITICAL);
    }
    
    // Set file logging if enabled
    if (config.toFile && !config.file.empty()) {
        setLogFile(config.file);
        
        // Set rotation parameters if maxSizeMb > 0
        if (config.maxSizeMb > 0) {
            setMaxFileSize(config.maxSizeMb * 1024 * 1024);  // Convert MB to bytes
            setMaxRotatedFiles(config.maxFiles);
        }
    }
    
    info(std::string("Logger initialized from config"));
}

// ============================================================================
// Scoped Logger
// ============================================================================

ScopedLogger::ScopedLogger(Logger& logger, LogLevel level, const std::string& context)
    : m_logger(logger), m_level(level), m_context(context), m_start(std::chrono::steady_clock::now()) {
    m_logger.log(m_level, context + " - enter");
}

ScopedLogger::~ScopedLogger() {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_start
    ).count();
    m_logger.log(m_level, m_context + " - exit (" + std::to_string(elapsed) + "ms)");
}

void ScopedLogger::log(const std::string& message) {
    m_logger.log(m_level, m_context + " - " + message);
}

// ============================================================================
// Template Implementations
// ============================================================================

namespace {

template<typename T>
void formatValue(std::string& output, T value) {
    output += std::to_string(value);
}

template<>
void formatValue(std::string& output, const char* value) {
    output += value;
}

template<>
void formatValue(std::string& output, const std::string& value) {
    output += value;
}

template<>
void formatValue(std::string& output, std::string_view value) {
    output += std::string(value);
}

} // anonymous namespace

} // namespace OmniNet
