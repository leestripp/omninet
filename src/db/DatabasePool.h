// OmniNet - Database Connection Pool
// Manages a pool of AsyncDatabase connections
#ifndef OMNINET_DB_DATABASE_POOL_H
#define OMNINET_DB_DATABASE_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../async/Executor.h"
#include "../async/Task.h"
#include "AsyncDatabase.h"

namespace OmniNet::DB {

// ============================================================================
// DatabasePool
// ============================================================================
//
// Manages a pool of AsyncDatabase connections for concurrent use.
// Connections are acquired/released by handlers. The pool maintains
// a minimum and maximum size, and recycles idle connections.
//
class DatabasePool {
public:
    struct Config {
        size_t minSize = 2;        // Initial pool size
        size_t maxSize = 16;       // Max connections
        std::chrono::seconds idleTimeout{300};  // Reclaim after idle
        std::chrono::seconds acquireTimeout{5};  // Max wait for connection
    };

    explicit DatabasePool(Async::Executor& exec, std::string path, Config config);
    DatabasePool(Async::Executor& exec, std::string path);  // Default config
    ~DatabasePool();

    // Non-copyable
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    // ----- Pool operations -----

    // Acquire a connection (blocking with timeout)
    std::shared_ptr<AsyncDatabase> acquire(
        std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Async acquire - waits if no connection available
    Async::Task<std::shared_ptr<AsyncDatabase>> acquire_async();

    // Release a connection back to the pool
    void release(std::shared_ptr<AsyncDatabase> db);

    // ----- Statistics -----

    struct Stats {
        size_t totalConnections;
        size_t inUse;
        size_t available;
        uint64_t totalAcquired;
        uint64_t totalReleased;
        uint64_t totalTimeouts;
    };

    Stats stats() const;

private:
    // Create a new connection (blocks until ready)
    std::shared_ptr<AsyncDatabase> create_connection();

    // Configuration
    Async::Executor& m_exec;
    std::string m_path;
    Config m_config;

    // All connections (in use + available)
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<std::shared_ptr<AsyncDatabase>> m_allConnections;
    std::vector<std::shared_ptr<AsyncDatabase>> m_available;
    std::atomic<bool> m_shutdown{false};

    // Statistics
    std::atomic<uint64_t> m_totalAcquired{0};
    std::atomic<uint64_t> m_totalReleased{0};
    std::atomic<uint64_t> m_totalTimeouts{0};
};

} // namespace OmniNet::DB

#endif // OMNINET_DB_DATABASE_POOL_H
