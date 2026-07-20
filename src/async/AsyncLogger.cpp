// OmniNet - AsyncLogger Implementation
#include "AsyncLogger.h"

#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace OmniNet::Async {

// ============================================================================
// Construction / Destruction
// ============================================================================

AsyncLogger::AsyncLogger() : AsyncLogger(Config{}) {}

AsyncLogger::AsyncLogger(Config config) : m_config(config) {
    m_running.store(true);
    m_shutdownRequested.store(false);

    // Use internal std::jthread
    m_writerThread = std::make_unique<std::jthread>(
        [this](std::stop_token) { writer_loop(); }
    );
}

AsyncLogger::AsyncLogger(Thread::ThreadPool& pool) : AsyncLogger(pool, Config{}) {}

AsyncLogger::AsyncLogger(Thread::ThreadPool& pool, Config config) : m_config(config) {
    m_running.store(true);
    m_shutdownRequested.store(false);

    // Use external thread pool - submit a long-running task
    m_pool = &pool;
    pool.submit([this]() {
        writer_loop();
    });
}

AsyncLogger::~AsyncLogger() {
    shutdown();
}

// ============================================================================
// Configuration
// ============================================================================

void AsyncLogger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFilePath = path;
    if (!m_logFilePath.empty()) {
        // Open in append mode
        m_logFile.open(m_logFilePath, std::ios::app);
    } else {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
}

void AsyncLogger::setLevel(LogLevel level) {
    m_minLevel = level;
}

// ============================================================================
// Logging
// ============================================================================

void AsyncLogger::log(LogLevel level, const std::string& message) {
    log(level, std::string_view(message));
}

void AsyncLogger::log(LogLevel level, std::string_view message) {
    if (!m_running.load()) return;
    if (static_cast<int>(level) < static_cast<int>(m_minLevel)) return;

    LogEntry entry{
        level,
        std::string(message),
        std::chrono::system_clock::now()
    };

    bool dropped = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_config.maxQueueSize) {
            // Queue full - drop the message
            dropped = true;
        } else {
            m_queue.push_back(std::move(entry));
        }
    }

    if (dropped) {
        m_stats.messagesDropped.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_stats.messagesQueued.fetch_add(1, std::memory_order_relaxed);
        m_cv.notify_one();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void AsyncLogger::flush() {
    // Wait until the queue is empty
    std::unique_lock<std::mutex> lock(m_mutex);
    m_flushCv.wait(lock, [this] {
        return m_queue.empty() || m_shutdownRequested.load();
    });
    m_stats.flushCount.fetch_add(1, std::memory_order_relaxed);
}

void AsyncLogger::shutdown() {
    if (!m_running.exchange(false)) {
        return;  // Already shut down
    }
    m_shutdownRequested.store(true);
    m_cv.notify_all();
    m_flushCv.notify_all();

    // If using internal jthread, join it
    if (m_writerThread && m_writerThread->joinable()) {
        m_writerThread->join();
    }
}

// ============================================================================
// Writer thread
// ============================================================================

void AsyncLogger::writer_loop() {
    while (m_running.load() || !m_queue.empty()) {
        std::deque<LogEntry> batch;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, m_config.flushInterval, [this] {
                return !m_queue.empty() || m_shutdownRequested.load();
            });
            // Drain all available messages
            batch.swap(m_queue);

            // v0.7.0 fix: write the batch INSIDE the lock so that
            // flush() — which checks m_queue.empty() — cannot
            // return via spurious wakeup before the file is
            // written. Without this, a spurious wakeup between
            // the swap and the file write would let flush()
            // return early, and the test (or any caller using
            // flush() to ensure the file is on disk) would see
            // stale/empty data. The cost: the lock is held
            // during the file write, but file writes are fast
            // for small batches. Console writes happen outside
            // the lock below to avoid blocking on slow terminals.
            if (!batch.empty()) {
                for (const auto& entry : batch) {
                    std::string formatted = format_entry(entry);
                    if (m_logFile.is_open()) {
                        m_logFile << formatted << std::endl;
                    }
                    m_stats.messagesWritten.fetch_add(1, std::memory_order_relaxed);
                }
                // Flush the file stream so the data is visible
                // to other file descriptors (e.g., an ifstream
                // opened by the test).
                if (m_logFile.is_open()) {
                    m_logFile.flush();
                }
                // Now safe to notify flush waiters — the file
                // has been written and flushed.
                m_flushCv.notify_all();
            }
        }

        // Console writes happen OUTSIDE the lock to avoid
        // blocking other threads on slow terminals.
        for (const auto& entry : batch) {
            std::string formatted = format_entry(entry);
            if (entry.level >= LogLevel::WARN) {
                std::cerr << formatted << std::endl;
            } else {
                std::cout << formatted << std::endl;
            }
        }
    }
}

void AsyncLogger::write_pending() {
    // Used for synchronous flush - not currently needed
}

// ============================================================================
// Formatting
// ============================================================================

std::string AsyncLogger::format_entry(const LogEntry& entry) {
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  entry.timestamp.time_since_epoch()) % 1000;

    // v0.6.0 fix (bug #7): use localtime_r instead of localtime.
    // AsyncLogger is by design multi-threaded (background writer
    // thread + calling threads); std::localtime's shared static
    // buffer is a data race.
    std::tm tm_buf{};
    ::localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << "[" << OmniNet::toString(entry.level) << "] "
        << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[" << std::this_thread::get_id() << "] "
        << entry.message;

    return oss.str();
}

} // namespace OmniNet::Async
