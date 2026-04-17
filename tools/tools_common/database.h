// OmniNet Tools - Database Access Helper
#ifndef OMNINET_TOOLS_DATABASE_H
#define OMNINET_TOOLS_DATABASE_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <variant>
#include <functional>
#include <fstream>
#include <regex>
#include <thread>

struct sqlite3;

namespace OmniNet::Tools {

// ============================================================================
// Database Connection
// ============================================================================

class Database {
public:
    Database();
    ~Database();
    
    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // Open database
    bool open(const std::string& path);
    void close();
    bool isOpen() const;
    
    // Check if database exists
    static bool exists(const std::string& path);
    
    // Get database path
    const std::string& path() const { return m_path; }
    
    // --- Query Execution ---
    
    // Execute SQL (for CREATE, INSERT, UPDATE, DELETE)
    bool execute(const std::string& sql);
    std::string lastError() const;
    
    // Query with callback
    using RowCallback = std::function<bool(const std::vector<std::string>& columns)>;
    bool query(const std::string& sql, RowCallback callback);
    
    // Query returning single row
    std::optional<std::vector<std::string>> queryOne(const std::string& sql);
    
    // Get single value
    std::optional<std::variant<std::string, int64_t, double>> queryValue(
        const std::string& sql);
    
    // Get last insert rowid
    int64_t lastInsertRowId() const;
    
    // Get affected rows
    int64_t affectedRows() const;
    
    // --- Schema Info ---
    
    std::vector<std::string> listTables();
    struct ColumnInfo {
        std::string name;
        std::string type;
        bool notNull = false;
        std::string defaultValue;
        bool primaryKey = false;
    };
    std::vector<ColumnInfo> tableColumns(const std::string& table);
    
    // --- Maintenance ---
    
    bool vacuum();
    bool analyze();
    
    // --- Backup ---
    
    bool backup(const std::string& destPath);
    bool restore(const std::string& srcPath);

private:
    sqlite3* m_db = nullptr;
    std::string m_path;
    std::string m_lastError;
};

// ============================================================================
// Intrusion Detection Data
// ============================================================================

struct IntrusionLogEntry {
    int64_t id = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string clientIp;
    std::string method;
    std::string path;
    std::string userAgent;
    int statusCode = 0;
    std::string reason;
    std::string rule;
};

struct BlockedIP {
    std::string ip;
    std::chrono::system_clock::time_point expires;
    std::chrono::system_clock::time_point blockedAt;
};

class IntrusionStore {
public:
    explicit IntrusionStore(Database& db);
    
    // Get blocked IPs
    std::vector<BlockedIP> getBlockedIPs();
    bool isBlocked(const std::string& ip);
    
    // Get recent logs
    std::vector<IntrusionLogEntry> getRecentLogs(size_t limit = 100);
    std::vector<IntrusionLogEntry> getLogsForIP(const std::string& ip, size_t limit = 50);
    
    // Block/Unblock
    bool blockIP(const std::string& ip, std::chrono::minutes duration);
    bool unblockIP(const std::string& ip);
    bool unblockAll();
    
    // Statistics
    struct Stats {
        size_t blockedCount = 0;
        size_t totalLogs = 0;
    };
    Stats stats() const;

private:
    Database& m_db;
};

// ============================================================================
// User/Session Data
// ============================================================================

struct User {
    int64_t id = 0;
    std::string username;
    std::string passwordHash;  // Only populated when needed
    std::string role;
    std::chrono::system_clock::time_point created;
    std::optional<std::chrono::system_clock::time_point> lastLogin;
};

struct Session {
    std::string id;
    int64_t userId = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expires;
};

class UserStore {
public:
    explicit UserStore(Database& db);
    
    // User operations
    std::vector<User> listUsers();
    std::optional<User> getUser(int64_t id);
    std::optional<User> getUserByUsername(const std::string& username);
    size_t userCount() const;
    
    // Create/Delete users (for omninet-init and omninet-users)
    bool createUser(const std::string& username, const std::string& passwordHash, const std::string& role = "user");
    bool deleteUser(int64_t userId);
    bool deleteUser(const std::string& username);
    bool updatePassword(int64_t userId, const std::string& passwordHash);
    bool updateLastLogin(int64_t userId);
    
    // Session operations
    std::vector<Session> getSessionsForUser(int64_t userId);
    std::vector<Session> getActiveSessions();
    bool revokeSession(const std::string& sessionId);
    bool revokeAllSessions(int64_t userId);
    
    // Ensure tables exist (called automatically if needed)
    bool ensureTables();
    
    // Statistics
    struct Stats {
        size_t userCount = 0;
        size_t activeSessions = 0;
    };
    Stats stats() const;

private:
    Database& m_db;
};

// ============================================================================
// Log Reader (for omninet-log)
// ============================================================================

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;  // DEBUG, INFO, WARN, ERROR, CRITICAL
    std::string message;
    std::string source;  // Optional source component
    int threadId = 0;
};

class LogReader {
public:
    LogReader();
    
    // Open log file
    bool open(const std::string& path);
    bool open();  // Use default path
    
    // Read entries
    std::vector<LogEntry> readAll();
    std::vector<LogEntry> readSince(const std::chrono::system_clock::time_point& since);
    std::vector<LogEntry> filter(const std::string& level);
    std::vector<LogEntry> grep(const std::string& pattern);
    
    // Statistics
    struct Stats {
        size_t totalLines = 0;
        size_t debugCount = 0;
        size_t infoCount = 0;
        size_t warnCount = 0;
        size_t errorCount = 0;
        size_t criticalCount = 0;
    };
    Stats stats() const;
    
    // Follow mode (like tail -f)
    using LineCallback = std::function<void(const LogEntry&)>;
    void follow(LineCallback callback, std::chrono::milliseconds pollInterval = std::chrono::milliseconds(500));
    void stopFollow();

private:
    std::string m_path;
    std::ifstream m_file;
    size_t m_lastPosition = 0;
    Stats m_stats;
    
    std::optional<LogEntry> parseLine(const std::string& line);
    void updateStats(const LogEntry& entry);
};

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_DATABASE_H
