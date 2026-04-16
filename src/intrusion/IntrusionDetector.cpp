// OmniNet - Intrusion Detector Implementation
#include "utils/Utils.h"
#include "IntrusionDetector.h"

namespace OmniNet {

// ============================================================================
// Default Suspicious Patterns
// ============================================================================

const std::vector<IntrusionDetector::SuspiciousPattern>& IntrusionDetector::defaultPatterns() {
    static std::vector<SuspiciousPattern> patterns = {
        // SQL Injection patterns
        {std::regex(R"(\b(union|select|insert|update|delete|drop|exec|execute)\b.*\b(from|table|database)\b)", std::regex::icase), "SQL injection attempt", 9},
        {std::regex(R"('|"|;|--|\bor\b|\band\b|\b1\s*=\s*1\b|\btrue\b)", std::regex::icase), "SQL injection character", 7},
        
        // Path traversal
        {std::regex(R"(\.\./|\.\.\\|%2e%2e%2f|%2e%2e/)", std::regex::icase), "Path traversal attempt", 8},
        {std::regex(R"(/etc/passwd|/etc/shadow|/windows/system32)", std::regex::icase), "Sensitive file access", 9},
        
        // Command injection
        {std::regex(R"([;&|`$]\s*(cat|ls|dir|rm|cp|mv|wget|curl|nc|bash|sh)\b)", std::regex::icase), "Command injection attempt", 10},
        {std::regex(R"(\|\s*\w+|>\s*/\w+|<\s*/\w+)", std::regex::icase), "Shell operator detected", 7},
        
        // Common exploit paths
        {std::regex(R"(/(\.env|\.git|\.htaccess|wp-config|config\.php|admin))", std::regex::icase), "Sensitive path access", 6},
        {std::regex(R"(/phpinfo|/server-status|/server-info)", std::regex::icase), "Server information leak", 5},
        
        // XSS patterns
        {std::regex(R"(<script|javascript:|onerror=|onclick=)", std::regex::icase), "XSS attempt", 8},
    };
    
    return patterns;
}

// ============================================================================
// Intrusion Detector
// ============================================================================

IntrusionDetector::IntrusionDetector(DB::Database& db) : m_db(db) {
    setupDefaultRules();
    
    // Create tables
    m_db.execute(R"(
        CREATE TABLE IF NOT EXISTS _intrusion_blocks (
            ip TEXT PRIMARY KEY,
            blocked_until TEXT NOT NULL,
            reason TEXT
        )
    )");
    
    m_db.execute(R"(
        CREATE TABLE IF NOT EXISTS _intrusion_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            client_ip TEXT NOT NULL,
            method TEXT,
            path TEXT NOT NULL,
            user_agent TEXT,
            status_code INTEGER,
            reason TEXT,
            rule TEXT
        )
    )");
    
    m_db.execute(R"(
        CREATE INDEX IF NOT EXISTS idx_intrusion_logs_ip ON _intrusion_logs(client_ip)
    )");
}

IntrusionDetector::~IntrusionDetector() = default;

void IntrusionDetector::setupDefaultRules() {
    m_rules = allDefaultRules();
}

bool IntrusionDetector::checkAndBlock(
    const std::string& clientIp,
    const std::string& method,
    const std::string& path,
    const HTTP::Request& req
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already blocked
    if (isBlocked(clientIp)) {
        return true;
    }
    
    // Check rules
    Rule matchedRule;
    if (matchesRules(method, path, matchedRule)) {
        // Log the attempt
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.clientIp = clientIp;
        entry.method = method;
        entry.path = path;
        entry.reason = matchedRule.description;
        entry.rule = matchedRule.name;
        persistLogEntry(entry);
        
        // Auto-block if enabled
        if (m_autoBlock && matchedRule.autoBlock) {
            blockIP(clientIp, matchedRule.blockDuration);
            return true;
        }
    }
    
    return false;
}

bool IntrusionDetector::isBlocked(const std::string& clientIp) const {
    // Check in-memory cache
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_blockExpiry.find(clientIp);
        if (it != m_blockExpiry.end()) {
            if (it->second > std::chrono::steady_clock::now()) {
                return true;
            }
            // Expired - will be cleaned up
        }
    }
    
    // Check database
    auto result = m_db.queryOne(
        "SELECT blocked_until FROM _intrusion_blocks WHERE ip = '" + clientIp + "'"
    );
    
    if (result) {
        // Parse and check
        return true;
    }
    
    return false;
}

void IntrusionDetector::blockIP(const std::string& ip, std::chrono::minutes duration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_blockedCache.insert(ip);
    m_blockExpiry[ip] = std::chrono::steady_clock::now() + duration;
    
    persistBlock(ip, duration);
}

void IntrusionDetector::unblockIP(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_blockedCache.erase(ip);
    m_blockExpiry.erase(ip);
    
    m_db.execute("DELETE FROM _intrusion_blocks WHERE ip = '" + ip + "'");
}

void IntrusionDetector::unblockAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_blockedCache.clear();
    m_blockExpiry.clear();
    
    m_db.execute("DELETE FROM _intrusion_blocks");
}

std::vector<std::string> IntrusionDetector::getBlockedIPs() const {
    std::vector<std::string> ips;
    
    m_db.query("SELECT ip FROM _intrusion_blocks", [&ips](const std::vector<std::string>& row) {
        ips.push_back(row[0]);
        return true;
    });
    
    return ips;
}

std::vector<IntrusionDetector::LogEntry> IntrusionDetector::getRecentLogs(size_t limit) {
    std::vector<LogEntry> entries;
    
    m_db.query(
        "SELECT id, timestamp, client_ip, method, path, user_agent, status_code, reason, rule "
        "FROM _intrusion_logs ORDER BY timestamp DESC LIMIT " + std::to_string(limit),
        [&entries](const std::vector<std::string>& row) {
            LogEntry entry;
            if (row.size() >= 9) {
                entry.id = std::stoll(row[0]);
                entry.clientIp = row[2];
                entry.method = row[3];
                entry.path = row[4];
                entry.userAgent = row[5];
                entry.statusCode = std::stoi(row[6]);
                entry.reason = row[7];
                entry.rule = row[8];
            }
            entries.push_back(entry);
            return true;
        }
    );
    
    return entries;
}

std::vector<IntrusionDetector::LogEntry> IntrusionDetector::getLogsForIP(
    const std::string& ip, size_t limit
) {
    std::vector<LogEntry> entries;
    
    m_db.query(
        "SELECT id, timestamp, client_ip, method, path, user_agent, status_code, reason, rule "
        "FROM _intrusion_logs WHERE client_ip = '" + ip + "' ORDER BY timestamp DESC LIMIT " + std::to_string(limit),
        [&entries](const std::vector<std::string>& row) {
            LogEntry entry;
            if (row.size() >= 9) {
                entry.id = std::stoll(row[0]);
                entry.clientIp = row[2];
                entry.method = row[3];
                entry.path = row[4];
                entry.userAgent = row[5];
                entry.statusCode = std::stoi(row[6]);
                entry.reason = row[7];
                entry.rule = row[8];
            }
            entries.push_back(entry);
            return true;
        }
    );
    
    return entries;
}

void IntrusionDetector::clearLogs(size_t keepCount) {
    m_db.execute(
        "DELETE FROM _intrusion_logs WHERE id NOT IN "
        "(SELECT id FROM _intrusion_logs ORDER BY timestamp DESC LIMIT " + std::to_string(keepCount) + ")"
    );
}

void IntrusionDetector::addRule(const Rule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rules.push_back(rule);
}

void IntrusionDetector::removeRule(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rules.erase(
        std::remove_if(m_rules.begin(), m_rules.end(),
            [&name](const Rule& r) { return r.name == name; }),
        m_rules.end()
    );
}

void IntrusionDetector::enableRule(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& rule : m_rules) {
        if (rule.name == name) {
            rule.enabled = enabled;
            break;
        }
    }
}

std::vector<IntrusionDetector::Rule> IntrusionDetector::getRules() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rules;
}

void IntrusionDetector::setAutoBlock(bool enabled) {
    m_autoBlock = enabled;
}

bool IntrusionDetector::autoBlock() const {
    return m_autoBlock;
}

void IntrusionDetector::setBlockDuration(std::chrono::minutes duration) {
    m_blockDuration = duration;
}

std::chrono::minutes IntrusionDetector::blockDuration() const {
    return m_blockDuration;
}

void IntrusionDetector::setMaxLogEntries(size_t max) {
    m_maxLogEntries = max;
}

size_t IntrusionDetector::maxLogEntries() const {
    return m_maxLogEntries;
}

void IntrusionDetector::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Remove expired blocks
    for (auto it = m_blockExpiry.begin(); it != m_blockExpiry.end(); ) {
        if (it->second <= now) {
            m_blockedCache.erase(it->first);
            it = m_blockExpiry.erase(it);
        } else {
            ++it;
        }
    }
}

IntrusionDetector::Stats IntrusionDetector::stats() const {
    Stats s;
    s.blockedIps = m_blockedCache.size();
    
    auto result = m_db.queryOne("SELECT COUNT(*) FROM _intrusion_logs");
    s.totalLogs = result ? std::stoul((*result)[0]) : 0;
    
    s.rulesCount = m_rules.size();
    
    return s;
}

bool IntrusionDetector::matchesRules(
    const std::string& method,
    const std::string& path,
    Rule& matchedRule
) {
    for (const auto& rule : m_rules) {
        if (!rule.enabled) continue;
        if (!rule.method.empty() && rule.method != method) continue;
        
        if (std::regex_search(path, rule.pattern)) {
            matchedRule = rule;
            return true;
        }
    }
    
    return false;
}

void IntrusionDetector::persistBlock(const std::string& ip, std::chrono::minutes duration) {
    auto expires = std::chrono::system_clock::now() + duration;
    auto expiresStr = Utils::formatRfc2822(expires);
    
    m_db.executef(
        "INSERT OR REPLACE INTO _intrusion_blocks (ip, blocked_until) VALUES ('%s', '%s')",
        ip.c_str(), expiresStr.c_str()
    );
}

void IntrusionDetector::persistLogEntry(const LogEntry& entry) {
    auto timestampStr = Utils::formatRfc2822(entry.timestamp);
    
    m_db.executef(
        "INSERT INTO _intrusion_logs (timestamp, client_ip, method, path, user_agent, status_code, reason, rule) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', %d, '%s', '%s')",
        timestampStr.c_str(),
        entry.clientIp.c_str(),
        entry.method.c_str(),
        entry.path.c_str(),
        entry.userAgent.c_str(),
        entry.statusCode,
        entry.reason.c_str(),
        entry.rule.c_str()
    );
}

// ============================================================================
// Pre-built Rule Sets
// ============================================================================

namespace IntrusionRules {

IntrusionDetector::Rule sqlInjectionRule() {
    return {
        "sql_injection",
        "SQL injection pattern detected",
        std::regex(R"((\b(union|select|insert|update|delete|drop)\b.*\b(from|table|database)\b|'|"|;|--|\bor\b|\band\b))", std::regex::icase),
        {}, // Any method
        9,
        true,
        std::chrono::minutes(30)
    };
}

IntrusionDetector::Rule pathTraversalRule() {
    return {
        "path_traversal",
        "Path traversal attempt detected",
        std::regex(R"(\.\./|\.\.\\|%2e%2e%2f|%2e%2e/)", std::regex::icase),
        {}, // Any method
        8,
        true,
        std::chrono::minutes(30)
    };
}

IntrusionDetector::Rule commandInjectionRule() {
    return {
        "command_injection",
        "Command injection attempt detected",
        std::regex(R"([;&|`$]\s*(cat|ls|dir|rm|cp|mv|wget|curl|nc|bash|sh)\b)", std::regex::icase),
        {}, // Any method
        10,
        true,
        std::chrono::minutes(60)
    };
}

IntrusionDetector::Rule exploitPathsRule() {
    return {
        "exploit_paths",
        "Common exploit path accessed",
        std::regex(R"(/(\.env|\.git|\.htaccess|wp-config|config\.php|admin))", std::regex::icase),
        {}, // Any method
        6,
        false, // Don't auto-block
        std::chrono::minutes(5)
    };
}

IntrusionDetector::Rule scrapingProtectionRule() {
    return {
        "scraping",
        "Excessive requests from single IP",
        std::regex(R"(^$)", std::regex::icase), // Placeholder
        {}, // Any method
        5,
        false,
        std::chrono::minutes(5)
    };
}

IntrusionDetector::Rule bruteForceRule() {
    return {
        "brute_force",
        "Multiple failed authentication attempts",
        std::regex(R"(^$)", std::regex::icase), // Placeholder
        "POST",
        8,
        true,
        std::chrono::minutes(15)
    };
}

std::vector<IntrusionDetector::Rule> allDefaultRules() {
    return {
        sqlInjectionRule(),
        pathTraversalRule(),
        commandInjectionRule(),
        exploitPathsRule()
    };
}

} // namespace IntrusionRules

// ============================================================================
// IP Allowlist
// ============================================================================

IPAllowlist::IPAllowlist() = default;

void IPAllowlist::add(const std::string& ipOrCidr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.insert(ipOrCidr);
}

void IPAllowlist::remove(const std::string& ipOrCidr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.erase(ipOrCidr);
}

bool IPAllowlist::isAllowed(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Exact match
    if (m_entries.count(ip)) return true;
    
    // CIDR match
    for (const auto& entry : m_entries) {
        if (entry.find('/') != std::string::npos) {
            if (Utils::matchesCidr(ip, entry)) {
                return true;
            }
        }
    }
    
    return false;
}

bool IPAllowlist::isAllowed(const std::string& ip, const IntrusionDetector& detector) const {
    if (isAllowed(ip)) return true;
    return !detector.isBlocked(ip);
}

// ============================================================================
// Middleware
// ============================================================================

IntrusionMiddleware::IntrusionMiddleware(IntrusionDetector& detector, IPAllowlist& allowlist)
    : m_detector(detector), m_allowlist(allowlist) {}

bool IntrusionMiddleware::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    auto ip = req.remoteAddress();
    
    // Check allowlist first
    if (!m_allowlist.isAllowed(ip, m_detector)) {
        resp = HTTP::Response::forbidden("Access denied");
        return false;
    }
    
    // Check intrusion detection
    if (m_detector.checkAndBlock(ip, toString(req.method()), std::string(req.path()), req)) {
        resp = HTTP::Response::forbidden("Blocked");
        return false;
    }
    
    return true;
}

} // namespace OmniNet