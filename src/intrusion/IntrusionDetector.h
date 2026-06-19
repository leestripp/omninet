// OmniNet - Intrusion Detection
#ifndef OMNINET_INTRUSION_DETECTOR_H
#define OMNINET_INTRUSION_DETECTOR_H

#include <chrono>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "db/Database.h"

namespace OmniNet {

// Forward declarations
namespace HTTP {
class Request;
class Response;
}

// ============================================================================
// Intrusion Detector
// ============================================================================

class IntrusionDetector {
public:
    // Log entry for suspicious activity
    struct LogEntry {
        int64_t id = 0;
        std::chrono::system_clock::time_point timestamp;
        std::string clientIp;
        std::string method;
        std::string path;
        std::string userAgent;
        int statusCode = 0;
        std::string reason;  // Why flagged
        std::string rule;    // Which rule matched
    };
    
    IntrusionDetector(DB::Database& db);
    ~IntrusionDetector();
    
    // --- Detection ---
    
    // Check request and determine if blocked
    // Returns true if request should be blocked
    bool checkAndBlock(
        const std::string& clientIp,
        const std::string& method,
        const std::string& path,
        const HTTP::Request* req = nullptr
    );
    
    // Check if IP is blocked
    bool isBlocked(const std::string& clientIp) const;
    
    // --- Blocking ---
    
    // Block IP manually
    void blockIP(const std::string& ip, std::chrono::minutes duration = std::chrono::minutes(30));
    
    // Unblock IP manually
    void unblockIP(const std::string& ip);
    
    // Unblock all IPs
    void unblockAll();
    
    // Get all blocked IPs
    std::vector<std::string> getBlockedIPs() const;
    
    // --- Logging ---
    
    // Get recent log entries
    std::vector<LogEntry> getRecentLogs(size_t limit = 100);
    
    // Get logs for specific IP
    std::vector<LogEntry> getLogsForIP(const std::string& ip, size_t limit = 50);
    
    // Clear old logs
    void clearLogs(size_t keepCount = 10000);
    
    // --- Rules ---
    
    struct Rule {
        std::string name;
        std::string description;
        std::regex pattern;      // Regex to match against path
        std::string method;      // Empty = any method
        int severity;            // 1-10 (10 = most severe)
        bool enabled = true;
        bool autoBlock = true;   // Automatically block on match
        std::chrono::minutes blockDuration{30};
    };
    
    // Add detection rule
    void addRule(const Rule& rule);
    
    // Remove rule by name
    void removeRule(const std::string& name);
    
    // Enable/disable rule
    void enableRule(const std::string& name, bool enabled);
    
    // Get all rules
    std::vector<Rule> getRules() const;
    
    // --- Configuration ---
    
    // Enable/disable auto-blocking
    void setAutoBlock(bool enabled);
    bool autoBlock() const;
    
    // Set default block duration
    void setBlockDuration(std::chrono::minutes duration);
    std::chrono::minutes blockDuration() const;
    
    // Set maximum log entries
    void setMaxLogEntries(size_t max);
    size_t maxLogEntries() const;
    
    // --- Maintenance ---
    
    // Cleanup expired blocks
    void cleanup();
    
    // Get statistics
    struct Stats {
        size_t blockedIps = 0;
        size_t totalLogs = 0;
        size_t rulesCount = 0;
    };
    
    Stats stats() const;

    // v0.7.0 (prompt.txt 2026-06-19 BUG-5): returns the full set
    // of CREATE TABLE / CREATE INDEX statements needed for the
    // IntrusionDetector. Useful for migration tools, schema
    // introspection, and documentation. The constructor calls
    // these statements itself, but external tools can also call
    // this method to apply the schema.
    static const std::vector<std::string>& schemaSQL();

private:
    bool matchesRules(
        const std::string& method,
        const std::string& path,
        Rule& matchedRule
    );

    void persistBlock(const std::string& ip, std::chrono::minutes duration);
    void persistLogEntry(const LogEntry& entry);

    // v0.7.0 (prompt.txt 2026-06-19 BUG-1): variant of isBlocked()
    // that ASSUMES the caller already holds m_mutex. Used internally
    // by checkAndBlock() to avoid a self-deadlock (std::mutex is
    // not recursive). Public callers should use isBlocked() (which
    // locks m_mutex).
    bool isBlockedUnlocked(const std::string& clientIp) const;
    
    DB::Database& m_db;
    std::vector<Rule> m_rules;
    std::unordered_set<std::string> m_blockedCache;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_blockExpiry;
    std::chrono::minutes m_blockDuration{30};
    bool m_autoBlock = true;
    size_t m_maxLogEntries = 10000;
    mutable std::mutex m_mutex;
    
    // Default rules
    void setupDefaultRules();
    
    // Parse suspicious patterns
    struct SuspiciousPattern {
        std::regex pattern;
        std::string description;
        int severity;
    };
    
    static const std::vector<SuspiciousPattern>& defaultPatterns();
};

// ============================================================================
// Pre-built Rule Sets
// ============================================================================

namespace IntrusionRules {

// SQL Injection patterns
IntrusionDetector::Rule sqlInjectionRule();

// Path traversal patterns
IntrusionDetector::Rule pathTraversalRule();

// Command injection patterns
IntrusionDetector::Rule commandInjectionRule();

// Common exploit paths
IntrusionDetector::Rule exploitPathsRule();

// Scraping protection
IntrusionDetector::Rule scrapingProtectionRule();

// Brute force protection
IntrusionDetector::Rule bruteForceRule();

// All default rules
std::vector<IntrusionDetector::Rule> allDefaultRules();

} // namespace IntrusionRules

// ============================================================================
// IP Allowlist
// ============================================================================

class IPAllowlist {
public:
    IPAllowlist();
    
    // Add IP or CIDR to allowlist
    void add(const std::string& ipOrCidr);
    
    // Remove from allowlist
    void remove(const std::string& ipOrCidr);
    
    // Check if IP is allowed
    bool isAllowed(const std::string& ip) const;
    
    // Check with reference to IntrusionDetector
    bool isAllowed(const std::string& ip, const IntrusionDetector& detector) const;

private:
    std::unordered_set<std::string> m_entries;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Middleware
// ============================================================================

class IntrusionMiddleware {
public:
    IntrusionMiddleware(IntrusionDetector& detector, IPAllowlist& allowlist);
    
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);

private:
    IntrusionDetector& m_detector;
    IPAllowlist& m_allowlist;
};

} // namespace OmniNet

#endif // OMNINET_INTRUSION_DETECTOR_H