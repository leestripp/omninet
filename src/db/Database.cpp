// OmniNet - Database Implementation
#include "Database.h"

#include <chrono>
#include <cstring>
#include <set>
#include <sstream>

// SQLite include
#include <sqlite3.h>

namespace OmniNet::DB {

// ============================================================================
// Database Exception
// ============================================================================

DatabaseException::DatabaseException(const std::string& message, int errorCode)
    : OmniNet::Exception(message, errorCode) {}

// ============================================================================
// Statement Implementation
// ============================================================================

struct Database::Statement::Impl {
    sqlite3_stmt* stmt = nullptr;
    sqlite3* db = nullptr;
    
    ~Impl() {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

Database::Statement::Statement(sqlite3* db, const std::string& sql)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->db = db;
    
    const char* tail = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &m_impl->stmt, &tail) != SQLITE_OK) {
        throw DatabaseException(sqlite3_errmsg(db), sqlite3_errcode(db));
    }
}

Database::Statement::~Statement() = default;

Database::Statement::Statement(Statement&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}

Database::Statement& Database::Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

void Database::Statement::bind(int index, std::nullptr_t) {
    sqlite3_bind_null(m_impl->stmt, index);
}

void Database::Statement::bind(int index, int value) {
    sqlite3_bind_int(m_impl->stmt, index, value);
}

void Database::Statement::bind(int index, int64_t value) {
    sqlite3_bind_int64(m_impl->stmt, index, value);
}

void Database::Statement::bind(int index, double value) {
    sqlite3_bind_double(m_impl->stmt, index, value);
}

void Database::Statement::bind(int index, const std::string& value) {
    sqlite3_bind_text(m_impl->stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Database::Statement::bind(int index, std::string_view value) {
    sqlite3_bind_text(m_impl->stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Database::Statement::bind(int index, std::span<const uint8_t> blob) {
    sqlite3_bind_blob(m_impl->stmt, index, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
}

void Database::Statement::reset() {
    sqlite3_reset(m_impl->stmt);
    sqlite3_clear_bindings(m_impl->stmt);
}

bool Database::Statement::step() {
    int result = sqlite3_step(m_impl->stmt);
    return result == SQLITE_ROW;
}

int Database::Statement::columnCount() const {
    return sqlite3_column_count(m_impl->stmt);
}

std::string Database::Statement::columnText(int index) const {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_impl->stmt, index));
    return text ? text : "";
}

int64_t Database::Statement::columnInt64(int index) const {
    return sqlite3_column_int64(m_impl->stmt, index);
}

double Database::Statement::columnDouble(int index) const {
    return sqlite3_column_double(m_impl->stmt, index);
}

std::span<const uint8_t> Database::Statement::columnBlob(int index) const {
    const void* blob = sqlite3_column_blob(m_impl->stmt, index);
    int size = sqlite3_column_bytes(m_impl->stmt, index);
    return std::span<const uint8_t>(static_cast<const uint8_t*>(blob), size);
}

bool Database::Statement::columnIsNull(int index) const {
    return sqlite3_column_type(m_impl->stmt, index) == SQLITE_NULL;
}

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database() = default;

Database::~Database() {
    close();
}

void Database::open(const std::string& path) {
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;
        throw DatabaseException("Failed to open database: " + error);
    }
    
    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON");
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::isOpen() const {
    return m_db != nullptr;
}

sqlite3* Database::handle() const {
    return m_db;
}

void Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);
    
    if (result != SQLITE_OK) {
        std::string error = errorMsg ? errorMsg : "Unknown error";
        if (errorMsg) sqlite3_free(errorMsg);
        throw DatabaseException(error, result);
    }
}

void Database::executef(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char* buffer = nullptr;
    vasprintf(&buffer, fmt, args);
    va_end(args);
    
    std::string sql(buffer);
    free(buffer);
    
    execute(sql);
}

void Database::query(const std::string& sql, RowCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Statement stmt(m_db, sql);
    
    while (stmt.step()) {
        std::vector<std::string> row;
        for (int i = 0; i < stmt.columnCount(); ++i) {
            row.push_back(stmt.columnText(i));
        }
        if (!callback(row)) {
            break;
        }
    }
}

std::optional<std::vector<std::string>> Database::queryOne(const std::string& sql) {
    std::optional<std::vector<std::string>> result;
    
    query(sql, [&result](const std::vector<std::string>& row) {
        result = row;
        return false;  // Stop after first row
    });
    
    return result;
}

// ============================================================================
// QueryResult Implementation
// ============================================================================

Database::QueryResult::QueryResult(Database& db, const std::string& sql)
    : m_db(db), m_stmt(std::make_unique<Statement>(db.createStatement(sql))), m_atEnd(false) {
    // Lock the database mutex for the duration of the query
    m_db.m_mutex.lock();
    // Fetch the first row
    m_hasCurrentRow = m_stmt->step();
    if (!m_hasCurrentRow) {
        m_atEnd = true;
        m_db.m_mutex.unlock();
    }
}

Database::QueryResult::~QueryResult() {
    if (!m_atEnd && m_stmt) {
        m_stmt->reset();
        m_db.m_mutex.unlock();
    }
}

Database::QueryResult::QueryResult(QueryResult&& other) noexcept
    : m_db(other.m_db)
    , m_stmt(std::move(other.m_stmt))
    , m_atEnd(other.m_atEnd)
    , m_currentRow(std::move(other.m_currentRow))
    , m_hasCurrentRow(other.m_hasCurrentRow) {
    other.m_atEnd = true;
    other.m_stmt.reset();
}

Database::QueryResult& Database::QueryResult::operator=(QueryResult&& other) noexcept {
    if (this != &other) {
        m_stmt = std::move(other.m_stmt);
        m_atEnd = other.m_atEnd;
        m_currentRow = std::move(other.m_currentRow);
        m_hasCurrentRow = other.m_hasCurrentRow;
        other.m_atEnd = true;
        other.m_stmt.reset();
    }
    return *this;
}

bool Database::QueryResult::fetchNext() {
    if (m_atEnd) return false;
    
    if (m_stmt->step()) {
        // Fetch row data
        m_currentRow.clear();
        for (int i = 0; i < m_stmt->columnCount(); ++i) {
            m_currentRow.push_back(m_stmt->columnText(i));
        }
        m_hasCurrentRow = true;
        return true;
    } else {
        // No more rows
        m_atEnd = true;
        m_stmt->reset();
        m_db.m_mutex.unlock();
        m_hasCurrentRow = false;
        return false;
    }
}

bool Database::QueryResult::hasResults() const {
    return m_hasCurrentRow && !m_atEnd;
}

// Iterator implementation
Database::QueryResult::Iterator::Iterator(QueryResult* result, bool end)
    : m_result(result), m_atEnd(end) {
    if (!end && result) {
        m_currentRow = result->m_currentRow;
    }
}

Database::QueryResult::Iterator::reference Database::QueryResult::Iterator::operator*() const {
    return m_currentRow;
}

Database::QueryResult::Iterator::pointer Database::QueryResult::Iterator::operator->() const {
    return &m_currentRow;
}

Database::QueryResult::Iterator& Database::QueryResult::Iterator::operator++() {
    if (m_result && !m_atEnd) {
        if (m_result->fetchNext()) {
            m_currentRow = m_result->m_currentRow;
        } else {
            m_atEnd = true;
        }
    }
    return *this;
}

Database::QueryResult::Iterator Database::QueryResult::Iterator::operator++(int) {
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool Database::QueryResult::Iterator::operator==(const Iterator& other) const {
    return m_atEnd == other.m_atEnd;
}

bool Database::QueryResult::Iterator::operator!=(const Iterator& other) const {
    return m_atEnd != other.m_atEnd;
}

Database::QueryResult::Iterator Database::QueryResult::begin() {
    return Iterator(this, false);
}

Database::QueryResult::Iterator Database::QueryResult::end() {
    return Iterator(this, true);
}

Database::QueryResult Database::query(const std::string& sql) {
    return QueryResult(*this, sql);
}

void Database::transaction(std::function<void()> fn) {
    begin();
    try {
        fn();
        commit();
    } catch (...) {
        rollback();
        throw;
    }
}

void Database::begin() {
    execute("BEGIN TRANSACTION");
    m_inTransaction = true;
}

void Database::commit() {
    execute("COMMIT");
    m_inTransaction = false;
}

void Database::rollback() {
    execute("ROLLBACK");
    m_inTransaction = false;
}

bool Database::inTransaction() const {
    return m_inTransaction;
}

Database::Statement Database::prepare(const std::string& sql) {
    return Statement(m_db, sql);
}

Database::Statement Database::createStatement(const std::string& sql) {
    return Statement(m_db, sql);
}

void Database::setBusyTimeout(std::chrono::milliseconds timeout) {
    sqlite3_busy_timeout(m_db, static_cast<int>(timeout.count()));
}

void Database::enableForeignKeys(bool enabled) {
    execute("PRAGMA foreign_keys = " + std::string(enabled ? "ON" : "OFF"));
}

void Database::enableWAL(bool enabled) {
    execute("PRAGMA journal_mode = " + std::string(enabled ? "WAL" : "DELETE"));
}

void Database::setCacheSize(size_t pages) {
    execute("PRAGMA cache_size = " + std::to_string(pages));
}

int64_t Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(m_db);
}

int64_t Database::affectedRows() const {
    return sqlite3_changes(m_db);
}

void Database::vacuum() {
    execute("VACUUM");
}

// ============================================================================
// Migration
// ============================================================================

bool Migration::Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

bool Migration::Version::operator==(const Version& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

Migration::Migration(const std::string& name, const std::string& up, const std::string& down)
    : m_name(name), m_up(up), m_down(down) {
    // Parse version from name (e.g., "001_initial_schema")
    std::istringstream iss(name);
    int major = 0, minor = 0, patch = 0;
    char dot = '\0';
    iss >> major >> dot >> minor >> dot >> patch;
    m_version = {major, minor, patch};
}

const std::string& Migration::name() const { return m_name; }
const Migration::Version& Migration::version() const { return m_version; }
const std::string& Migration::up() const { return m_up; }
const std::string& Migration::down() const { return m_down; }

void runMigrations(Database& db, const std::vector<Migration>& migrations) {
    // Create migrations table
    db.execute(R"(
        CREATE TABLE IF NOT EXISTS _schema_migrations (
            version TEXT PRIMARY KEY,
            applied_at TEXT NOT NULL
        )
    )");
    
    // Get applied migrations
    std::set<Migration::Version> applied;
    db.query("SELECT version FROM _schema_migrations", [&applied](const std::vector<std::string>& row) {
        applied.insert(Migration::Version{});  // TODO: parse version
        return true;
    });
    
    // Apply pending migrations
    for (const auto& mig : migrations) {
        if (applied.find(mig.version()) == applied.end()) {
            db.transaction([&]() {
                db.execute(mig.up());
                db.executef(
                    "INSERT INTO _schema_migrations (version, applied_at) VALUES ('%s', datetime('now'))",
                    mig.name().c_str()
                );
            });
        }
    }
}

} // namespace OmniNet::DB