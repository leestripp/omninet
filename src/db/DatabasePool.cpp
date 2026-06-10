// OmniNet - Database Connection Pool Implementation
#include "DatabasePool.h"

#include <chrono>

namespace OmniNet::DB {

// ============================================================================
// Construction / Destruction
// ============================================================================

DatabasePool::DatabasePool(Async::Executor& exec, std::string path) : DatabasePool(exec, std::move(path), Config{}) {}

DatabasePool::DatabasePool(Async::Executor& exec, std::string path, Config config)
    : m_exec(exec), m_path(std::move(path)), m_config(config) {
    // Pre-create minimum connections
    for (size_t i = 0; i < m_config.minSize; ++i) {
        try {
            m_allConnections.push_back(create_connection());
            m_available.push_back(m_allConnections.back());
        } catch (const std::exception& e) {
            // Log and continue with whatever we got
        }
    }
}

DatabasePool::~DatabasePool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown.store(true);
        // Mark all connections as closed
        for (auto& db : m_allConnections) {
            if (db) db->markClosed();
        }
        m_available.clear();
        m_allConnections.clear();
    }
    m_cv.notify_all();
}

// ============================================================================
// Pool operations
// ============================================================================

std::shared_ptr<AsyncDatabase> DatabasePool::acquire(
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(m_mutex);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        if (m_shutdown.load()) {
            m_totalTimeouts.fetch_add(1);
            return nullptr;
        }

        // Check available connections
        if (!m_available.empty()) {
            auto db = m_available.back();
            m_available.pop_back();
            if (db && !db->isClosed()) {
                db->setInUse(true);
                m_totalAcquired.fetch_add(1);
                return db;
            }
            // Skip dead connections
            continue;
        }

        // Try to create a new one if under max
        if (m_allConnections.size() < m_config.maxSize) {
            lock.unlock();
            try {
                auto db = create_connection();
                lock.lock();
                m_allConnections.push_back(db);
                db->setInUse(true);
                m_totalAcquired.fetch_add(1);
                return db;
            } catch (...) {
                lock.lock();
                // Fall through to wait
            }
        }

        // Wait for a connection to be released
        if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            m_totalTimeouts.fetch_add(1);
            return nullptr;
        }
    }
}

Async::Task<std::shared_ptr<AsyncDatabase>> DatabasePool::acquire_async() {
    // Dispatch the blocking acquire to the executor
    auto fut = m_exec.submit([this]() {
        return acquire(std::chrono::duration_cast<std::chrono::milliseconds>(
            m_config.acquireTimeout));
    });

    co_return fut.get();
}

void DatabasePool::release(std::shared_ptr<AsyncDatabase> db) {
    if (!db) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        db->setInUse(false);
        if (!m_shutdown.load() && !db->isClosed()) {
            m_available.push_back(db);
        }
    }
    m_totalReleased.fetch_add(1);
    m_cv.notify_one();
}

std::shared_ptr<AsyncDatabase> DatabasePool::create_connection() {
    auto db = std::make_shared<AsyncDatabase>(m_exec, m_path);
    return db;
}

DatabasePool::Stats DatabasePool::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Stats s;
    s.totalConnections = m_allConnections.size();
    s.available = m_available.size();
    s.inUse = s.totalConnections - s.available;
    s.totalAcquired = m_totalAcquired.load();
    s.totalReleased = m_totalReleased.load();
    s.totalTimeouts = m_totalTimeouts.load();
    return s;
}

} // namespace OmniNet::DB
