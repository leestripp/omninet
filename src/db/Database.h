// OmniNet - Database (SQLite3 wrapper)
#ifndef OMNINET_DATABASE_H
#define OMNINET_DATABASE_H

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "utils/Exception.h"

// Forward declare sqlite3
struct sqlite3;

namespace OmniNet::DB {

// ============================================================================
// Database Exception
// ============================================================================

class DatabaseException : public OmniNet::Exception {
public:
    DatabaseException(const std::string& message, int errorCode = 0);
};

// ============================================================================
// Database
// ============================================================================

class Database {
public:
    Database();
    ~Database();
    
    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // --- Lifecycle ---
    
    // Open database file
    void open(const std::string& path);
    
    // Close database
    void close();
    
    // Check if open
    bool isOpen() const;
    
    // Get underlying handle
    sqlite3* handle() const;
    
    // --- Execution ---
    
    // Execute SQL (for CREATE, INSERT, UPDATE, DELETE)
    void execute(const std::string& sql);
    
    // Execute with format
    void executef(const char* fmt, ...);
    
    // --- Queries ---
    
    // Query with callback for each row
    // Callback returns false to stop iteration
    using RowCallback = std::function<bool(const std::vector<std::string>& columns)>;
    void query(const std::string& sql, RowCallback callback);
    
    // Query returning first row
    std::optional<std::vector<std::string>> queryOne(const std::string& sql);
    
    // --- Transactions ---
    
    // Execute in transaction
    void transaction(std::function<void()> fn);
    
    // Manual transaction control
    void begin();
    void commit();
    void rollback();
    
    // Check if in transaction
    bool inTransaction() const;
    
    // --- Prepared Statements ---
    
    class Statement {
    public:
        ~Statement();
        
        // Prevent copying
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
        
        // Movable
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&& other) noexcept;
        
        // --- Binding ---
        
        void bind(int index, std::nullptr_t);
        void bind(int index, int value);
        void bind(int index, int64_t value);
        void bind(int index, double value);
        void bind(int index, const std::string& value);
        void bind(int index, std::string_view value);
        void bind(int index, std::span<const uint8_t> blob);
        
        // Bind by name
        void bind(const std::string& name, auto value);
        
        // --- Iteration ---
        
        // Step to next row (returns false if no more rows)
        bool step();
        
        // Reset to beginning
        void reset();
        
        // --- Column Access ---
        
        int columnCount() const;
        
        std::string columnText(int index) const;
        int64_t columnInt64(int index) const;
        double columnDouble(int index) const;
        std::span<const uint8_t> columnBlob(int index) const;
        bool columnIsNull(int index) const;
        
        // Named column access
        std::string columnText(const std::string& name) const;
        
    private:
        friend class Database;
        Statement(sqlite3* db, const std::string& sql);
        
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
    
    // Prepare statement
    Statement prepare(const std::string& sql);
    
    // --- Configuration ---
    
    // Set busy timeout
    void setBusyTimeout(std::chrono::milliseconds timeout);
    
    // Enable foreign keys
    void enableForeignKeys(bool enabled);
    
    // Enable WAL mode
    void enableWAL(bool enabled);
    
    // Set cache size
    void setCacheSize(size_t pages);
    
    // --- Utility ---
    
    // Get last inserted row ID
    int64_t lastInsertRowId() const;
    
    // Get number of affected rows
    int64_t affectedRows() const;
    
    // Vacuum database
    void vacuum();

private:
    sqlite3* m_db = nullptr;
    bool m_inTransaction = false;
    std::mutex m_mutex;
};

// ============================================================================
// Schema Migration
// ============================================================================

class Migration {
public:
    struct Version {
        int major;
        int minor;
        int patch;
        
        bool operator<(const Version& other) const;
        bool operator==(const Version& other) const;
    };
    
    Migration(const std::string& name, const std::string& up, const std::string& down);
    
    const std::string& name() const;
    const Version& version() const;
    const std::string& up() const;
    const std::string& down() const;

private:
    std::string m_name;
    Version m_version;
    std::string m_up;
    std::string m_down;
};

// Run migrations on database
void runMigrations(Database& db, const std::vector<Migration>& migrations);

} // namespace OmniNet::DB

#endif // OMNINET_DATABASE_H
