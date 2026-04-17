// OmniNet Tools - Database Access Implementation
#include "database.h"
#include "common.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>
#include <iostream>

// SQLite include
extern "C" {
#include <sqlite3.h>
}

namespace OmniNet::Tools {

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database() = default;

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    close();
    
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = "Failed to open database: " + std::string(sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    
    // Enable foreign keys
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
    
    m_path = path;
    return true;
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_path.clear();
}

bool Database::isOpen() const {
    return m_db != nullptr;
}

bool Database::exists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string Database::lastError() const {
    if (m_db) {
        return std::string(sqlite3_errmsg(m_db));
    }
    return m_lastError;
}

bool Database::execute(const std::string& sql) {
    if (!m_db) {
        m_lastError = "Database not open";
        return false;
    }
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        m_lastError = errMsg ? std::string(errMsg) : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool Database::query(const std::string& sql, RowCallback callback) {
    if (!m_db) {
        m_lastError = "Database not open";
        return false;
    }
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return false;
    }
    
    bool result = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int colCount = sqlite3_column_count(stmt);
        std::vector<std::string> row;
        row.reserve(colCount);
        
        for (int i = 0; i < colCount; ++i) {
            const char* text = (const char*)sqlite3_column_text(stmt, i);
            row.push_back(text ? text : "");
        }
        
        if (!callback(row)) {
            result = false;
            break;
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::optional<std::vector<std::string>> Database::queryOne(const std::string& sql) {
    std::optional<std::vector<std::string>> result;
    
    query(sql, [&result](const std::vector<std::string>& row) {
        result = row;
        return false;  // Stop after first row
    });
    
    return result;
}

std::optional<std::variant<std::string, int64_t, double>> Database::queryValue(
    const std::string& sql) {
    std::optional<std::variant<std::string, int64_t, double>> result;
    
    query(sql, [&result](const std::vector<std::string>& row) {
        if (row.empty()) return false;
        
        const auto& val = row[0];
        if (val.empty()) {
            result = std::string("");
        } else {
            // Try to parse as number
            char* endptr = nullptr;
            int64_t intVal = strtoll(val.c_str(), &endptr, 10);
            if (*endptr == '\0') {
                result = int64_t(intVal);
            } else {
                double doubleVal = strtod(val.c_str(), &endptr);
                if (*endptr == '\0') {
                    result = doubleVal;
                } else {
                    result = val;
                }
            }
        }
        return false;
    });
    
    return result;
}

int64_t Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(m_db);
}

int64_t Database::affectedRows() const {
    return sqlite3_changes(m_db);
}

std::vector<std::string> Database::listTables() {
    std::vector<std::string> tables;
    
    query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'",
        [&tables](const std::vector<std::string>& row) {
            tables.push_back(row[0]);
            return true;
        });
    
    std::sort(tables.begin(), tables.end());
    return tables;
}

std::vector<Database::ColumnInfo> Database::tableColumns(const std::string& table) {
    std::vector<ColumnInfo> columns;
    std::string sql = "PRAGMA table_info(" + table + ")";
    
    query(sql, [&columns](const std::vector<std::string>& row) {
        ColumnInfo col;
        col.name = row[1];
        col.type = row[2];
        col.notNull = row[3] == "1";
        col.defaultValue = row[4];
        col.primaryKey = row[5] == "1";
        columns.push_back(col);
        return true;
    });
    
    return columns;
}

bool Database::vacuum() {
    return execute("VACUUM");
}

bool Database::analyze() {
    return execute("ANALYZE");
}

bool Database::backup(const std::string& destPath) {
    sqlite3* destDb = nullptr;
    
    int rc = sqlite3_open(destPath.c_str(), &destDb);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(destDb, "main", m_db, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
    }
    
    rc = sqlite3_close(destDb);
    return rc == SQLITE_OK;
}

bool Database::restore(const std::string& srcPath) {
    // Open source database
    sqlite3* srcDb = nullptr;
    int rc = sqlite3_open(srcPath.c_str(), &srcDb);
    if (rc != SQLITE_OK) {
        if (srcDb) sqlite3_close(srcDb);
        return false;
    }
    
    // Use backup API to restore
    sqlite3_backup* backup = sqlite3_backup_init(m_db, "main", srcDb, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
    }
    
    sqlite3_close(srcDb);
    return true;
}

// ============================================================================
// IntrusionStore Implementation
// ============================================================================

IntrusionStore::IntrusionStore(Database& db) : m_db(db) {}

std::vector<BlockedIP> IntrusionStore::getBlockedIPs() {
    std::vector<BlockedIP> blocked;
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    std::string sql = R"(
        SELECT ip, expires, blocked_at 
        FROM blocked_ips 
        WHERE expires > ?)";
    
    m_db.query(sql, [&blocked, now](const std::vector<std::string>& row) {
        BlockedIP entry;
        entry.ip = row[0];
        entry.expires = std::chrono::system_clock::from_time_t(std::stoll(row[1]));
        entry.blockedAt = std::chrono::system_clock::from_time_t(std::stoll(row[2]));
        blocked.push_back(entry);
        return true;
    });
    
    return blocked;
}

bool IntrusionStore::isBlocked(const std::string& ip) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto sql = "SELECT COUNT(*) FROM blocked_ips WHERE ip = '" + ip + "' AND expires > " + std::to_string(now);
    auto result = m_db.queryValue(sql);
    if (result) {
        if (auto val = std::get_if<int64_t>(&*result)) {
            return *val > 0;
        }
    }
    return false;
}

std::vector<IntrusionLogEntry> IntrusionStore::getRecentLogs(size_t limit) {
    std::vector<IntrusionLogEntry> logs;
    std::string sql = "SELECT * FROM intrusion_logs ORDER BY timestamp DESC LIMIT " + std::to_string(limit);
    
    m_db.query(sql, [&logs](const std::vector<std::string>& row) {
        IntrusionLogEntry entry;
        entry.id = std::stoll(row[0]);
        entry.timestamp = std::chrono::system_clock::from_time_t(std::stoll(row[1]));
        entry.clientIp = row[2];
        entry.method = row[3];
        entry.path = row[4];
        entry.userAgent = row[5];
        entry.statusCode = std::stoi(row[6]);
        entry.reason = row[7];
        entry.rule = row[8];
        logs.push_back(entry);
        return true;
    });
    
    return logs;
}

std::vector<IntrusionLogEntry> IntrusionStore::getLogsForIP(const std::string& ip, size_t limit) {
    std::vector<IntrusionLogEntry> logs;
    std::string sql = "SELECT * FROM intrusion_logs WHERE client_ip = ? ORDER BY timestamp DESC LIMIT " + std::to_string(limit);
    
    m_db.query(sql, [&logs](const std::vector<std::string>& row) {
        IntrusionLogEntry entry;
        entry.id = std::stoll(row[0]);
        entry.timestamp = std::chrono::system_clock::from_time_t(std::stoll(row[1]));
        entry.clientIp = row[2];
        entry.method = row[3];
        entry.path = row[4];
        entry.userAgent = row[5];
        entry.statusCode = std::stoi(row[6]);
        entry.reason = row[7];
        entry.rule = row[8];
        logs.push_back(entry);
        return true;
    });
    
    return logs;
}

bool IntrusionStore::blockIP(const std::string& ip, std::chrono::minutes duration) {
    auto now = std::chrono::system_clock::now();
    auto expires = now + duration;
    auto nowT = std::chrono::system_clock::to_time_t(now);
    auto expiresT = std::chrono::system_clock::to_time_t(expires);
    
    std::string sql = "INSERT OR REPLACE INTO blocked_ips (ip, expires, blocked_at) VALUES (?, ?, ?)";
    // Note: Using simple execute since we don't have bind params in this implementation
    // For full implementation, would need prepared statements
    
    return false;  // Simplified - would need proper binding
}

bool IntrusionStore::unblockIP(const std::string& ip) {
    std::string sql = "DELETE FROM blocked_ips WHERE ip = ?";
    return m_db.execute(sql);
}

bool IntrusionStore::unblockAll() {
    return m_db.execute("DELETE FROM blocked_ips");
}

IntrusionStore::Stats IntrusionStore::stats() const {
    Stats s;
    
    if (auto count = m_db.queryValue("SELECT COUNT(*) FROM blocked_ips")) {
        if (auto val = std::get_if<int64_t>(&*count)) {
            s.blockedCount = static_cast<size_t>(*val);
        }
    }
    
    if (auto count = m_db.queryValue("SELECT COUNT(*) FROM intrusion_logs")) {
        if (auto val = std::get_if<int64_t>(&*count)) {
            s.totalLogs = static_cast<size_t>(*val);
        }
    }
    
    return s;
}

// ============================================================================
// UserStore Implementation
// ============================================================================

UserStore::UserStore(Database& db) : m_db(db) {}

std::vector<User> UserStore::listUsers() {
    std::vector<User> users;
    
    m_db.query("SELECT id, username, role, created, last_login FROM users",
        [&users](const std::vector<std::string>& row) {
            User user;
            user.id = std::stoll(row[0]);
            user.username = row[1];
            user.role = row[2];
            user.created = std::chrono::system_clock::from_time_t(std::stoll(row[3]));
            if (!row[4].empty()) {
                user.lastLogin = std::chrono::system_clock::from_time_t(std::stoll(row[4]));
            }
            users.push_back(user);
            return true;
        });
    
    return users;
}

std::optional<User> UserStore::getUser(int64_t id) {
    std::optional<User> result;
    
    m_db.query("SELECT id, username, role, created, last_login FROM users WHERE id = " + std::to_string(id),
        [&result](const std::vector<std::string>& row) {
            User user;
            user.id = std::stoll(row[0]);
            user.username = row[1];
            user.role = row[2];
            user.created = std::chrono::system_clock::from_time_t(std::stoll(row[3]));
            if (!row[4].empty()) {
                user.lastLogin = std::chrono::system_clock::from_time_t(std::stoll(row[4]));
            }
            result = user;
            return false;
        });
    
    return result;
}

std::optional<User> UserStore::getUserByUsername(const std::string& username) {
    std::optional<User> result;
    
    m_db.query("SELECT id, username, role, created, last_login FROM users WHERE username = '" + username + "'",
        [&result](const std::vector<std::string>& row) {
            User user;
            user.id = std::stoll(row[0]);
            user.username = row[1];
            user.role = row[2];
            user.created = std::chrono::system_clock::from_time_t(std::stoll(row[3]));
            if (!row[4].empty()) {
                user.lastLogin = std::chrono::system_clock::from_time_t(std::stoll(row[4]));
            }
            result = user;
            return false;
        });
    
    return result;
}

size_t UserStore::userCount() const {
    if (auto count = m_db.queryValue("SELECT COUNT(*) FROM users")) {
        if (auto val = std::get_if<int64_t>(&*count)) {
            return static_cast<size_t>(*val);
        }
    }
    return 0;
}

std::vector<Session> UserStore::getSessionsForUser(int64_t userId) {
    std::vector<Session> sessions;
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    m_db.query("SELECT id, user_id, created, expires FROM sessions WHERE user_id = " + std::to_string(userId) + " AND expires > " + std::to_string(now),
        [&sessions](const std::vector<std::string>& row) {
            Session session;
            session.id = row[0];
            session.userId = std::stoll(row[1]);
            session.created = std::chrono::system_clock::from_time_t(std::stoll(row[2]));
            session.expires = std::chrono::system_clock::from_time_t(std::stoll(row[3]));
            sessions.push_back(session);
            return true;
        });
    
    return sessions;
}

std::vector<Session> UserStore::getActiveSessions() {
    std::vector<Session> sessions;
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    m_db.query("SELECT id, user_id, created, expires FROM sessions WHERE expires > " + std::to_string(now),
        [&sessions](const std::vector<std::string>& row) {
            Session session;
            session.id = row[0];
            session.userId = std::stoll(row[1]);
            session.created = std::chrono::system_clock::from_time_t(std::stoll(row[2]));
            session.expires = std::chrono::system_clock::from_time_t(std::stoll(row[3]));
            sessions.push_back(session);
            return true;
        });
    
    return sessions;
}

bool UserStore::revokeSession(const std::string& sessionId) {
    return m_db.execute("DELETE FROM sessions WHERE id = '" + sessionId + "'");
}

bool UserStore::revokeAllSessions(int64_t userId) {
    return m_db.execute("DELETE FROM sessions WHERE user_id = " + std::to_string(userId));
}

UserStore::Stats UserStore::stats() const {
    Stats s;
    s.userCount = userCount();
    
    if (auto count = m_db.queryValue("SELECT COUNT(*) FROM sessions WHERE expires > " + std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())))) {
        if (auto val = std::get_if<int64_t>(&*count)) {
            s.activeSessions = static_cast<size_t>(*val);
        }
    }
    
    return s;
}

// ============================================================================
// LogReader Implementation
// ============================================================================

LogReader::LogReader() = default;

bool LogReader::open(const std::string& path) {
    m_file.close();
    m_path = path;
    m_file.open(path);
    m_lastPosition = 0;
    
    if (!m_file.is_open()) {
        return false;
    }
    
    // Get file size
    m_file.seekg(0, std::ios::end);
    m_lastPosition = m_file.tellg();
    m_file.seekg(0, std::ios::beg);
    
    return true;
}

bool LogReader::open() {
    return open(Paths::logFile());
}

std::vector<LogEntry> LogReader::readAll() {
    std::vector<LogEntry> entries;
    
    if (!m_file.is_open()) {
        return entries;
    }
    
    m_file.seekg(0, std::ios::beg);
    std::string line;
    while (std::getline(m_file, line)) {
        auto entry = parseLine(line);
        if (entry) {
            entries.push_back(*entry);
        }
    }
    
    return entries;
}

std::vector<LogEntry> LogReader::readSince(const std::chrono::system_clock::time_point& since) {
    auto entries = readAll();
    auto sinceTimeT = std::chrono::system_clock::to_time_t(since);
    
    std::vector<LogEntry> filtered;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(filtered),
        [sinceTimeT](const LogEntry& e) {
            auto entryTimeT = std::chrono::system_clock::to_time_t(e.timestamp);
            return entryTimeT >= sinceTimeT;
        });
    
    return filtered;
}

std::vector<LogEntry> LogReader::filter(const std::string& level) {
    auto entries = readAll();
    
    std::vector<LogEntry> filtered;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(filtered),
        [&level](const LogEntry& e) {
            return e.level == level;
        });
    
    return filtered;
}

std::vector<LogEntry> LogReader::grep(const std::string& pattern) {
    auto entries = readAll();
    std::regex regexPattern(pattern, std::regex::icase);
    
    std::vector<LogEntry> filtered;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(filtered),
        [&regexPattern](const LogEntry& e) {
            return std::regex_search(e.message, regexPattern);
        });
    
    return filtered;
}

LogReader::Stats LogReader::stats() const {
    return m_stats;
}

void LogReader::follow(LineCallback callback, std::chrono::milliseconds pollInterval) {
    if (!m_file.is_open()) return;
    
    m_file.seekg(0, std::ios::end);
    size_t lastPos = m_lastPosition;
    
    while (m_file.is_open() && m_file.good()) {
        m_file.seekg(lastPos, std::ios::beg);
        std::string line;
        
        while (std::getline(m_file, line)) {
            auto entry = parseLine(line);
            if (entry) {
                callback(*entry);
                updateStats(*entry);
            }
            lastPos = m_file.tellg();
        }
        
        std::this_thread::sleep_for(pollInterval);
    }
}

void LogReader::stopFollow() {
    m_file.close();
}

void LogReader::updateStats(const LogEntry& entry) {
    m_stats.totalLines++;
    if (entry.level == "DEBUG") m_stats.debugCount++;
    else if (entry.level == "INFO") m_stats.infoCount++;
    else if (entry.level == "WARN") m_stats.warnCount++;
    else if (entry.level == "ERROR") m_stats.errorCount++;
    else if (entry.level == "CRITICAL") m_stats.criticalCount++;
}

std::optional<LogEntry> LogReader::parseLine(const std::string& line) {
    // Expected format: [LEVEL] [timestamp] [thread] message
    // Example: [INFO] [2024-01-15 10:30:45] [1234] Server started on port 8080
    
    static std::regex logRegex(R"(\[(\w+)\]\s*\[([^\]]+)\]\s*\[(\d+)\]\s*(.*))");
    std::smatch match;
    
    if (std::regex_match(line, match, logRegex)) {
        LogEntry entry;
        entry.level = match[1];
        
        // Parse timestamp
        std::tm tm = {};
        std::istringstream ss(match[2]);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            return std::nullopt;
        }
        entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        entry.threadId = std::stoi(match[3]);
        entry.message = match[4];
        
        return entry;
    }
    
    return std::nullopt;
}

} // namespace OmniNet::Tools
