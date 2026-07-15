// OmniNet - Async Database
// Async wrapper around DB::Database using coroutines + ThreadPool
#ifndef OMNINET_DB_ASYNC_DATABASE_H
#define OMNINET_DB_ASYNC_DATABASE_H

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../async/Executor.h"
#include "../async/Task.h"
#include "../io/IoContext.h"
#include "Database.h"

namespace OmniNet::DB {

// Forward declaration
class AsyncDatabasePool;

// ============================================================================
// AsyncDatabase
// ============================================================================
//
// Async wrapper around a synchronous Database. All operations are dispatched
// to a thread pool, so the calling coroutine can be suspended without
// blocking the event loop.
//
// Usage:
//   Async::Task<std::vector<Row>> get_users() {
//       AsyncDatabase db(...);
//       co_return co_await db.query("SELECT * FROM users");
//   }
//
class AsyncDatabase {
public:
    AsyncDatabase(Async::Executor& exec, std::string path);
    ~AsyncDatabase();

    // Non-copyable
    AsyncDatabase(const AsyncDatabase&) = delete;
    AsyncDatabase& operator=(const AsyncDatabase&) = delete;

    // Non-movable: holds an Async::Executor& reference. The DatabasePool
    // passes these around as std::shared_ptr, so move semantics aren't
    // needed. v0.8.7 was defaulting these, which the compiler silently
    // deleted (-> warning -Wdefaulted-function-deleted) because you
    // can't move-ref bind a reference member.
    AsyncDatabase(AsyncDatabase&&) noexcept = delete;
    AsyncDatabase& operator=(AsyncDatabase&&) noexcept = delete;

    // ----- Async operations -----

    // Execute a query and get all rows
    Async::Task<std::vector<std::vector<std::string>>> query(std::string_view sql);

    // Execute a single-row query
    Async::Task<std::optional<std::vector<std::string>>> query_one(std::string_view sql);

    // Execute SQL that doesn't return rows
    Async::Task<void> execute(std::string_view sql);

    // ----- std::expected variants (delegated to native Database) -----

    // Execute SQL with type-safe error handling
    [[nodiscard]] OmniNet::Result<void, OmniNet::DbError>
    tryExecute(std::string_view sql);

    // Query returning first row with type-safe error handling
    [[nodiscard]] OmniNet::Result<std::optional<std::vector<std::string>>, OmniNet::DbError>
    tryQueryOne(std::string_view sql);

    // ----- Synchronous access (use sparingly) -----

    // Get the underlying database (for prepared statements, etc.)
    Database& native() { return m_db; }
    const Database& native() const { return m_db; }

    // Get the path
    const std::string& path() const { return m_path; }

    // Check if open
    bool isOpen() const { return m_db.isOpen(); }

    // Mark as in use (for pool)
    void setInUse(bool inUse) { m_inUse.store(inUse); }
    bool inUse() const { return m_inUse.load(); }

    // Mark as closed (for pool cleanup)
    void markClosed() { m_closed.store(true); }
    bool isClosed() const { return m_closed.load(); }

private:
    Async::Executor& m_exec;
    Database m_db;
    std::string m_path;
    std::atomic<bool> m_inUse{false};
    std::atomic<bool> m_closed{false};
};

} // namespace OmniNet::DB

#endif // OMNINET_DB_ASYNC_DATABASE_H
