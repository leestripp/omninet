// OmniNet - Dashboard Handler
#ifndef OMNINET_DASHBOARD_H
#define OMNINET_DASHBOARD_H

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "http/HttpServer.h"
#include "db/SessionStore.h"
#include "db/UserStore.h"
#include "dashboard/RateLimiter.h"
#include "utils/LogReader.h"

namespace OmniNet::Dashboard {

// ============================================================================
// Dashboard Handler
// ============================================================================

class Handler {
public:
    // Configuration
    struct Config {
        std::string urlPath = "/omninet";
        std::string apiPath = "/omninet/api";
        std::string dbPath;
        std::vector<std::string> allowedIpPrefixes = {"127.", "::1"};
        int sessionTtlSeconds = 3600;
        size_t maxLoginAttempts = 5;
        bool enabled = true;
        
        // Resolve path (e.g., for dbPath)
        static std::string resolve(const std::string& path);
        
        Config();
    };
    
    explicit Handler(const Config& config);
    
    // Mount on HTTP server
    void mount(HTTP::Server& server);
    
    // Handle request (for manual integration)
    bool handleRequest(const HTTP::Request& req, HTTP::Response& resp);
    
    // --- Access Control ---
    
    // Check if request is from allowed IP
    bool isAllowedIp(const std::string& ip) const;
    
    // Add allowed IP prefix
    void addAllowedIp(const std::string& prefix);
    
    // Remove allowed IP prefix
    void removeAllowedIp(const std::string& prefix);
    
    // --- User Management ---
    
    DB::UserStore& users();
    DB::SessionStore& sessions();
    
    // --- Statistics Store ---
    
    class StatsStore {
    public:
        void set(const std::string& key, const std::string& value);
        void set(const std::string& key, int64_t value);
        void increment(const std::string& key, int64_t delta = 1);
        
        std::optional<std::string> get(const std::string& key) const;
        std::optional<int64_t> getInt(const std::string& key) const;
        
        void remove(const std::string& key);
        std::vector<std::string> keys() const;
        
    private:
        friend class Handler;
    };
    
    StatsStore& stats();
    
    // --- Customization ---
    
    void setCustomCSS(const std::string& css);
    void setCustomJS(const std::string& js);
    
private:
    // Route handlers
    void handleLogin(const HTTP::Request& req, HTTP::Response& resp);
    void handleLogout(const HTTP::Request& req, HTTP::Response& resp);
    void handleGetUsers(const HTTP::Request& req, HTTP::Response& resp);
    void handleCreateUser(const HTTP::Request& req, HTTP::Response& resp);
    void handleUpdateUser(const HTTP::Request& req, HTTP::Response& resp);
    void handleDeleteUser(const HTTP::Request& req, HTTP::Response& resp);
    void handleChangePassword(const HTTP::Request& req, HTTP::Response& resp);
    void handleGetStats(const HTTP::Request& req, HTTP::Response& resp);
    void handleRecordStat(const HTTP::Request& req, HTTP::Response& resp);
    void handleGetLogs(const HTTP::Request& req, HTTP::Response& resp);
    void serveDashboardPage(const HTTP::Request& req, HTTP::Response& resp);
    void serveLoginPage(const HTTP::Request& req, HTTP::Response& resp);
    
    // Auth helpers
    std::optional<std::string> extractSessionId(const HTTP::Request& req);
    std::optional<DB::UserStore::User> getAuthenticatedUser(const HTTP::Request& req);
    void requireAuth(const HTTP::Request& req);
    void requireAdmin(const HTTP::Request& req);
    
    // Serve page
    std::string renderLoginPage(const std::string& error = {});
    std::string renderDashboardPage(const DB::UserStore::User& user);
    
    Config m_config;
    std::unique_ptr<DB::Database> m_db;
    std::unique_ptr<DB::SessionStore> m_sessions;
    std::unique_ptr<DB::UserStore> m_users;
    std::unique_ptr<StatsStore> m_stats;
    std::unique_ptr<RateLimiter> m_rateLimiter;
    
    std::string m_customCSS;
    std::string m_customJS;
    
    // Log reader for /omninet/logs endpoint
    std::unique_ptr<LogReader> m_logReader;
    
    // HTML templates
    std::string m_loginPage;
    std::string m_dashboardPage;
    
    // Start time for uptime calculation
    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace OmniNet::Dashboard

#endif // OMNINET_DASHBOARD_H
