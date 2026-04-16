// OmniNet - Logging system
#ifndef OMNINET_LOGGER_H
#define OMNINET_LOGGER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace OmniNet {

// ============================================================================
// Log Level
// ============================================================================

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRITICAL = 4
};

inline const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARN:      return "WARN";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

// ============================================================================
// Logger Singleton
// ============================================================================

class Logger {
public:
    // Get singleton instance
    static Logger& instance();
    
    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // --- Configuration ---
    
    // Set minimum log level (default: INFO)
    void setLevel(LogLevel level);
    LogLevel level() const;
    
    // Set log file path (empty = console only)
    void setLogFile(const std::string& path);
    const std::optional<std::string>& logFile() const;
    
    // Set async mode (default: true for better performance)
    void setAsync(bool async);
    bool isAsync() const;
    
    // --- Logging Methods ---
    
    void log(LogLevel level, const std::string& message);
    void log(LogLevel level, std::string_view message);
    
    void debug(const std::string& message);
    void debug(std::string_view message);
    
    void info(const std::string& message);
    void info(std::string_view message);
    
    void warn(const std::string& message);
    void warn(std::string_view message);
    
    void error(const std::string& message);
    void error(std::string_view message);
    
    void critical(const std::string& message);
    void critical(std::string_view message);
    
    // --- Formatting ---
    
    // Format a log entry: "[LEVEL] [timestamp] [thread] message"
    std::string format(LogLevel level, const std::string& message);
    std::string format(LogLevel level, std::string_view message);
    
    // --- Custom Handlers ---
    
    using Handler = std::function<void(LogLevel, const std::string&)>;
    void setHandler(Handler handler);
    
    // --- Flushing ---
    
    // Flush pending logs (call before exit if async)
    void flush();
    
    // --- Rotation Support ---
    
    // Set max log file size (0 = disabled)
    void setMaxFileSize(size_t bytes);
    
    // Set max number of rotated files (0 = disabled)
    void setMaxRotatedFiles(size_t count);
    
    // Force rotation
    void rotate();
    
    // --- Convenience for streams ---
    
    template<typename... Args>
    void logF(LogLevel level, const char* format, Args&&... args);
    
    template<typename... Args>
    void infoF(const char* format, Args&&... args);
    
    template<typename... Args>
    void warnF(const char* format, Args&&... args);
    
    template<typename... Args>
    void errorF(const char* format, Args&&... args);

private:
    Logger();
    ~Logger();
    
    void writeLog(LogLevel level, const std::string& formatted);
    void performRotation();
    
    // Async logging support
    struct AsyncLogEntry {
        LogLevel level;
        std::string message;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void asyncWorker();
    void enqueueLog(AsyncLogEntry entry);
    
    std::atomic<LogLevel> m_level{LogLevel::INFO};
    std::optional<std::string> m_logFile;
    bool m_async = true;
    
    Handler m_handler;
    
    // File rotation
    std::atomic<size_t> m_maxFileSize{0};
    std::atomic<size_t> m_maxRotatedFiles{0};
    
    // Async queue
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::vector<AsyncLogEntry> m_queue;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
};

// ============================================================================
// Template Implementations
// ============================================================================

// Simple snprintf-style formatter (avoids dependency on fmt library)
template<typename... Args>
inline void formatTo(std::string& output, const char* format, Args&&... args);

template<typename... Args>
inline std::string formatString(const char* format, Args&&... args) {
    std::string result;
    result.reserve(256);
    formatTo(result, format, std::forward<Args>(args)...);
    return result;
}

template<typename... Args>
inline void Logger::logF(LogLevel level, const char* format, Args&&... args) {
    log(level, formatString(format, std::forward<Args>(args)...));
}

template<typename... Args>
inline void Logger::infoF(const char* format, Args&&... args) {
    info(formatString(format, std::forward<Args>(args)...));
}

template<typename... Args>
inline void Logger::warnF(const char* format, Args&&... args) {
    warn(formatString(format, std::forward<Args>(args)...));
}

template<typename... Args>
inline void Logger::errorF(const char* format, Args&&... args) {
    error(formatString(format, std::forward<Args>(args)...));
}

// ============================================================================
// Inline Logging Helpers (convenience macros alternative)
// ============================================================================

// ScopedLogger - logs entry/exit for a scope
class ScopedLogger {
public:
    ScopedLogger(Logger& logger, LogLevel level, const std::string& context);
    ~ScopedLogger();
    
    void log(const std::string& message);

private:
    Logger& m_logger;
    LogLevel m_level;
    std::string m_context;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace OmniNet

// ============================================================================
// Convenience Macros (optional usage)
// ============================================================================

// Note: These are optional - you can use Logger::instance() directly
#define OMNINET_LOG_DEBUG(msg) OmniNet::Logger::instance().debug(msg)
#define OMNINET_LOG_INFO(msg) OmniNet::Logger::instance().info(msg)
#define OMNINET_LOG_WARN(msg) OmniNet::Logger::instance().warn(msg)
#define OMNINET_LOG_ERROR(msg) OmniNet::Logger::instance().error(msg)
#define OMNINET_LOG_CRITICAL(msg) OmniNet::Logger::instance().critical(msg)

#define OMNET_SCOPED_LOGGER(ctx) OmniNet::ScopedLogger OmniNet_UNIQ_NAME(_logger)(OmniNet::Logger::instance(), OmniNet::LogLevel::INFO, ctx)

// Implementation details for scoped logger
#define OmniNet_UNIQ_NAME_IMPL(_counter, _name) _name##_counter
#define OmniNet_UNIQ_NAME(_counter, _name) OmniNet_UNIQ_NAME_IMPL(_counter, _name)

#endif // OMNINET_LOGGER_H
