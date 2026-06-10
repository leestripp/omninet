// OmniNet - Async Database Implementation
#include "AsyncDatabase.h"

#include <iostream>

namespace OmniNet::DB {

// ============================================================================
// Construction / Destruction
// ============================================================================

AsyncDatabase::AsyncDatabase(Async::Executor& exec, std::string path)
    : m_exec(exec), m_db(), m_path(std::move(path)) {
    m_db.open(m_path);
}

AsyncDatabase::~AsyncDatabase() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

// ============================================================================
// Async operations
// ============================================================================

Async::Task<std::vector<std::vector<std::string>>>
AsyncDatabase::query(std::string_view sql) {
    std::string sqlStr(sql);

    // Dispatch blocking call to thread pool
    auto fut = m_exec.submit([this, sqlStr]() {
        std::vector<std::vector<std::string>> rows;
        m_db.query(sqlStr, [&rows](const std::vector<std::string>& cols) {
            rows.push_back(cols);
            return true;
        });
        return rows;
    });

    // Wait for the result
    co_return fut.get();
}

Async::Task<std::optional<std::vector<std::string>>>
AsyncDatabase::query_one(std::string_view sql) {
    std::string sqlStr(sql);

    auto fut = m_exec.submit([this, sqlStr]() {
        return m_db.queryOne(sqlStr);
    });

    co_return fut.get();
}

Async::Task<void>
AsyncDatabase::execute(std::string_view sql) {
    std::string sqlStr(sql);

    auto fut = m_exec.submit([this, sqlStr]() {
        m_db.execute(sqlStr);
    });

    fut.wait();
    co_return;
}

OmniNet::Result<void, OmniNet::DbError>
AsyncDatabase::tryExecute(std::string_view sql) {
    // Delegate to the native Database's tryExecute
    return m_db.tryExecute(std::string(sql));
}

OmniNet::Result<std::optional<std::vector<std::string>>, OmniNet::DbError>
AsyncDatabase::tryQueryOne(std::string_view sql) {
    // Delegate to the native Database's tryQueryOne
    return m_db.tryQueryOne(std::string(sql));
}

} // namespace OmniNet::DB
