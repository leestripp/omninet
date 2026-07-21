// OmniNet - AsyncLogger
// Non-blocking logging via MPSC queue + background writer thread
#ifndef OMNINET_ASYNC_ASYNC_LOGGER_H
#define OMNINET_ASYNC_ASYNC_LOGGER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "../thread/ThreadPool.h"
#include "../utils/Logger.h"

namespace OmniNet::Async {

// ============================================================================
// AsyncLogger
// ============================================================================
//
// Non-blocking logger that queues messages and writes them on a background
// thread. Safe to call from multiple threads; the calling thread never
// blocks on disk I/O.
//
// Architecture:
//   1. log() enqueues messages (under mutex, very fast)
//   2. Background writer thread drains the queue and writes to disk
//   3. flush() blocks until the queue is empty
//   4. shutdown() flushes pending messages and stops the writer
//
// Backward compat: existing Logger::instance() still works; AsyncLogger
// is a separate, optional component.
//
class AsyncLogger {
public:
    // --- Configuration ---

    struct Config {
        // Maximum number of queued messages before dropping (default 10k)
        size_t maxQueueSize = 10'000;
        // Flush interval for the background writer (default 100ms)
        std::chrono::milliseconds flushInterval{100};
        // Auto-flush on log (default false - only when buffer is full)
        bool autoFlush = false;
    };

    // --- Statistics ---

    struct Stats {
        std::atomic<uint64_t> messagesQueued{0};
        std::atomic<uint64_t> messagesWritten{0};
        std::atomic<uint64_t> messagesDropped{0};
        std::atomic<uint64_t> flushCount{0};
        std::atomic<bool> running{false};
    };

    // --- Construction ---

    // Create AsyncLogger with optional thread pool (uses std::thread if null)
    explicit AsyncLogger(Config config);
    AsyncLogger();  // Default config
    AsyncLogger(Thread::ThreadPool& pool, Config config);
    AsyncLogger(Thread::ThreadPool& pool);  // Default config + external pool
    ~AsyncLogger();

    // Non-copyable, non-movable
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    // --- Configuration ---

    // Set log file path (empty = no file output, only console)
    void setLogFile(const std::string& path);

    // Set minimum log level (default: INFO)
    void setLevel(LogLevel level);
    LogLevel level() const { return m_minLevel; }

    // --- Logging ---

    // Non-blocking log (copies the message)
    void log(LogLevel level, const std::string& message);
    void log(LogLevel level, std::string_view message);

    // Convenience methods
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg)  { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg)  { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void critical(const std::string& msg) { log(LogLevel::CRITICAL, msg); }

    // --- Lifecycle ---

    // Flush all pending messages (blocking)
    void flush();

    // Shutdown: flush and stop the writer thread
    void shutdown();

    // Check if running
    bool running() const { return m_running.load(); }

    // Get statistics
    const Stats& stats() const { return m_stats; }

private:
    // Log entry
    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    // Writer thread main loop
    void writer_loop();

    // Write all pending messages
    void write_pending();

    // Format a log entry
    static std::string format_entry(const LogEntry& entry);

    // Configuration
    Config m_config;
    LogLevel m_minLevel = LogLevel::INFO;
    std::string m_logFilePath;
    std::ofstream m_logFile;

    // External thread pool (optional)
    Thread::ThreadPool* m_pool = nullptr;

    // Future for the writer task submitted to the external pool.
    std::future<void> m_poolTask;

    // Writer thread (when no external pool)
    std::unique_ptr<std::jthread> m_writerThread;

    // Message queue
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_flushCv;
    std::deque<LogEntry> m_queue;

    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shutdownRequested{false};

    // Statistics
    Stats m_stats;
};

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_ASYNC_LOGGER_H
