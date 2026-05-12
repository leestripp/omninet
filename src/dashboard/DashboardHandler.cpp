// OmniNet - Dashboard Handler Implementation
#include "DashboardHandler.h"
#include "utils/Utils.h"
#include "utils/Exception.h"
#include "config/Config.h"

#include <filesystem>

namespace OmniNet::Dashboard {
using OmniNet::Utils::split;
using OmniNet::Utils::urlDecode;

// ============================================================================
// Handler
// ============================================================================

std::string Handler::Config::resolve(const std::string& path) {
    std::filesystem::path p(path);
    if (p.is_absolute()) return path;
    return (std::filesystem::current_path() / p).string();
}

Handler::Config::Config() = default;

Handler::Handler(const Config& config)
    : m_config(config)
    , m_startTime(std::chrono::steady_clock::now()) {
    
    // Open database
    std::string dbPath = config.dbPath.empty() ? 
        Config::resolve("dashboard.db") : config.dbPath;
    
    m_db = std::make_unique<DB::Database>();
    m_db->open(dbPath);
    
    m_sessions = std::make_unique<DB::SessionStore>(*m_db);
    m_sessions->initialize();
    
    m_users = std::make_unique<DB::UserStore>(*m_db);
    m_users->initialize();
    
    m_stats = std::make_unique<StatsStore>();
    m_rateLimiter = std::make_unique<RateLimiter>(
        config.maxLoginAttempts,
        std::chrono::minutes(15)
    );
    
    // Initialize log reader - use ~/.omninet/logs/omninet.log
    auto dataDir = OmniNet::Config::instance().dataDir();
    auto logPath = dataDir / "logs" / "omninet.log";
    if (std::filesystem::exists(logPath)) {
        m_logReader = std::make_unique<LogReader>(logPath.string());
    }
}

void Handler::mount(HTTP::Server& server) {
    // Serve dashboard page
    server.get(m_config.urlPath, [this](const HTTP::Request& req, HTTP::Response& resp) {
        serveDashboardPage(req, resp);
    });
    
    // API endpoints
    server.post(m_config.apiPath + "/login", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleLogin(req, resp);
    });
    
    server.post(m_config.apiPath + "/logout", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleLogout(req, resp);
    });
    
    server.get(m_config.apiPath + "/users", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleGetUsers(req, resp);
    });
    
    server.post(m_config.apiPath + "/users", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleCreateUser(req, resp);
    });
    
    server.delete_(m_config.apiPath + "/users/:id", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleDeleteUser(req, resp);
    });
    
    server.get(m_config.apiPath + "/stats", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleGetStats(req, resp);
    });
    
    server.get(m_config.apiPath + "/logs", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleGetLogs(req, resp);
    });
}

bool Handler::handleRequest(const HTTP::Request& req, HTTP::Response& resp) {
    // Check IP
    auto ip = std::string(req.remoteAddress());
    if (!m_config.allowedIpPrefixes.empty() && !isAllowedIp(std::string(ip))) {
        resp = HTTP::Response::forbidden("Access denied from this IP");
        return true;
    }
    
    return false;
}

bool Handler::isAllowedIp(const std::string& ip) const {
    for (const auto& prefix : m_config.allowedIpPrefixes) {
        if (ip.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

void Handler::addAllowedIp(const std::string& prefix) {
    m_config.allowedIpPrefixes.push_back(prefix);
}

void Handler::removeAllowedIp(const std::string& prefix) {
    auto it = std::find(m_config.allowedIpPrefixes.begin(), 
                        m_config.allowedIpPrefixes.end(), prefix);
    if (it != m_config.allowedIpPrefixes.end()) {
        m_config.allowedIpPrefixes.erase(it);
    }
}

DB::UserStore& Handler::users() {
    return *m_users;
}

DB::SessionStore& Handler::sessions() {
    return *m_sessions;
}

void Handler::setCustomCSS(const std::string& css) {
    m_customCSS = css;
}

void Handler::setCustomJS(const std::string& js) {
    m_customJS = js;
}

// ============================================================================
// Route Handlers
// ============================================================================

void Handler::handleLogin(const HTTP::Request& req, HTTP::Response& resp) {
    auto ip = std::string(req.remoteAddress());
    
    // Check if blocked
    if (m_rateLimiter->isBlocked(ip)) {
        resp = HTTP::Response::tooManyRequests("Too many failed attempts");
        return;
    }
    
    // Get credentials from body
    auto body = req.bodyAsString();
    auto params = Utils::split(body, '&');
    
    std::string username, password;
    for (const auto& param : params) {
        auto kv = Utils::split(param, '=');
        if (kv.size() >= 2) {
            if (kv[0] == "username") username = Utils::urlDecode(kv[1]);
            if (kv[0] == "password") password = Utils::urlDecode(kv[1]);
        }
    }
    
    // Verify credentials
    auto user = m_users->verify(username, password);
    
    if (!user) {
        m_rateLimiter->recordFailure(ip);
        resp = HTTP::Response::unauthorized("Invalid credentials");
        return;
    }
    
    // Clear rate limit
    m_rateLimiter->recordSuccess(ip);
    
    // Record login
    m_users->recordLogin(user->id);
    
    // Create session
    auto sessionId = m_sessions->create(
        std::to_string(user->id),
        std::chrono::seconds(m_config.sessionTtlSeconds)
    );
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({"success": true, "session": ")" + sessionId + R"(", "user": ")" + user->username + R"("})");
}

void Handler::handleLogout(const HTTP::Request& req, HTTP::Response& resp) {
    auto sessionId = extractSessionId(req);
    if (sessionId) {
        m_sessions->remove(*sessionId);
    }
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({"success": true})");
}

void Handler::handleGetUsers(const HTTP::Request& req, HTTP::Response& resp) {
    // Require admin
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        resp = HTTP::Response::forbidden("Admin access required");
        return;
    }
    
    auto users = m_users->list();
    
    std::string json = "[";
    for (size_t i = 0; i < users.size(); ++i) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(users[i].id);
        json += R"(,"username":")" + users[i].username + R"(")";
        json += R"(,"role":")" + users[i].role + R"("})";
    }
    json += "]";
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(json);
}

void Handler::handleCreateUser(const HTTP::Request& req, HTTP::Response& resp) {
    // Require admin
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        resp = HTTP::Response::forbidden("Admin access required");
        return;
    }
    
    // Get params from body
    auto body = req.bodyAsString();
    auto params = Utils::split(body, '&');
    
    std::string username, password, role;
    for (const auto& param : params) {
        auto kv = Utils::split(param, '=');
        if (kv.size() >= 2) {
            if (kv[0] == "username") username = Utils::urlDecode(kv[1]);
            if (kv[0] == "password") password = Utils::urlDecode(kv[1]);
            if (kv[0] == "role") role = Utils::urlDecode(kv[1]);
        }
    }
    
    if (username.empty() || password.empty()) {
        resp = HTTP::Response::badRequest("Username and password required");
        return;
    }
    
    auto id = m_users->createUser(username, password, role.empty() ? "user" : role);
    
    resp.setStatus(HTTP::StatusCode::CREATED);
    resp.setContentType("application/json");
    resp.setBody(R"({"success": true, "id":)" + std::to_string(id) + "}");
}

void Handler::handleUpdateUser(const HTTP::Request& req, HTTP::Response& resp) {
    // Require admin
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        resp = HTTP::Response::forbidden("Admin access required");
        return;
    }
    
    resp = HTTP::Response::notImplemented();
}

void Handler::handleDeleteUser(const HTTP::Request& req, HTTP::Response& resp) {
    // Require admin
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        resp = HTTP::Response::forbidden("Admin access required");
        return;
    }
    
    auto idStr = req.pathParam(0);
    auto id = std::stoll(std::string(idStr));
    
    m_users->remove(id);
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({"success": true})");
}

void Handler::handleChangePassword(const HTTP::Request& req, HTTP::Response& resp) {
    // Get authenticated user
    auto user = getAuthenticatedUser(req);
    if (!user) {
        resp = HTTP::Response::unauthorized("Authentication required");
        return;
    }
    
    // Get params from body
    auto body = req.bodyAsString();
    auto params = Utils::split(body, '&');
    
    std::string currentPassword, newPassword;
    for (const auto& param : params) {
        auto kv = Utils::split(param, '=');
        if (kv.size() >= 2) {
            if (kv[0] == "currentPassword") currentPassword = Utils::urlDecode(kv[1]);
            if (kv[0] == "newPassword") newPassword = Utils::urlDecode(kv[1]);
        }
    }
    
    if (currentPassword.empty() || newPassword.empty()) {
        resp = HTTP::Response::badRequest("Current and new password required");
        return;
    }
    
    // Verify current password
    auto verified = m_users->verify(user->username, currentPassword);
    if (!verified) {
        resp = HTTP::Response::unauthorized("Current password is incorrect");
        return;
    }
    
    // Validate new password strength (at least 6 characters)
    if (newPassword.length() < 6) {
        resp = HTTP::Response::badRequest("New password must be at least 6 characters");
        return;
    }
    
    // Update password
    try {
        m_users->updatePassword(user->id, newPassword);
        resp.setStatus(HTTP::StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"success": true, "message": "Password changed successfully"})");
    } catch (const std::exception& e) {
        resp = HTTP::Response::internalError("Failed to change password");
    }
}

void Handler::handleGetStats(const HTTP::Request& req, HTTP::Response& resp) {
    (void)req;
    
    // Gather stats from various sources
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream body;
    body << "{\n";
    body << "  \"timestamp\": \"" << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ") << "\",\n";
    body << "  \"uptime\": " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_startTime).count() << ",\n";
    body << "  \"users\": {\n";
    body << "    \"total\": " << m_users->count() << "\n";
    body << "  },\n";
    body << "  \"sessions\": {\n";
    body << "    \"active\": " << m_sessions->count() << "\n";
    body << "  },\n";
    body << "  \"rateLimit\": {\n";
    body << "    \"maxLoginAttempts\": " << m_rateLimiter->maxAttempts() << "\n";
    body << "  },\n";
    body << "  \"version\": \"" << OmniNet::Admin::Handler::version() << "\"\n";
    body << "}\n";
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(body.str());
}

void Handler::handleGetLogs(const HTTP::Request& req, HTTP::Response& resp) {
    // Check if log reader is available
    if (!m_logReader) {
        resp.setStatus(static_cast<HTTP::StatusCode>(503));
        resp.setContentType("application/json");
        resp.setBody(R"({"error": "Log file not configured"})");
        return;
    }
    
    // Parse query parameters
    size_t limit = 100;
    std::optional<LogLevel> minLevel;
    std::string pattern;
    
    const auto& query = req.query();
    auto limitParam = query.get("limit");
    if (limitParam) {
        try {
            limit = std::stoul(std::string(*limitParam));
            if (limit > 1000) limit = 1000;  // Cap at 1000
        } catch (...) {}
    }
    
    auto levelParam = query.get("level");
    if (levelParam) {
        std::string level = std::string(*levelParam);
        if (level == "DEBUG") minLevel = LogLevel::DEBUG;
        else if (level == "INFO") minLevel = LogLevel::INFO;
        else if (level == "WARN") minLevel = LogLevel::WARN;
        else if (level == "ERROR") minLevel = LogLevel::ERROR;
        else if (level == "CRITICAL") minLevel = LogLevel::CRITICAL;
    }
    
    auto patternParam = query.get("pattern");
    if (patternParam) {
        pattern = std::string(*patternParam);
    }
    
    // Apply filters
    if (minLevel) {
        m_logReader->setMinLevel(*minLevel);
    }
    if (!pattern.empty()) {
        m_logReader->setPattern(pattern);
    }
    
    // Read log entries
    auto entries = m_logReader->readLast(limit);
    
    // Build JSON response
    std::ostringstream json;
    json << "{\"logs\":[";
    bool first = true;
    for (const auto& entry : entries) {
        if (!first) json << ",";
        json << entry.toJson();
        first = false;
    }
    json << "],\"count\":" << entries.size() << "}";
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(json.str());
}

void Handler::handleRecordStat(const HTTP::Request& req, HTTP::Response& resp) {
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({"success": true})");
}

void Handler::serveDashboardPage(const HTTP::Request& req, HTTP::Response& resp) {
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("text/html");
    resp.setBody(renderDashboardPage(DB::UserStore::User{}));
}

void Handler::serveLoginPage(const HTTP::Request& req, HTTP::Response& resp) {
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("text/html");
    resp.setBody(renderLoginPage());
}

// ============================================================================
// Auth Helpers
// ============================================================================

std::optional<std::string> Handler::extractSessionId(const HTTP::Request& req) {
    auto header = req.header("X-Session-ID");
    if (!header) {
        header = req.header("Authorization");
        if (header && header->starts_with("Bearer ")) {
            return std::string(header->substr(7));
        }
    }
    return header ? std::optional<std::string>(std::string(*header)) : std::nullopt;
}

std::optional<DB::UserStore::User> Handler::getAuthenticatedUser(const HTTP::Request& req) {
    auto sessionId = extractSessionId(req);
    if (!sessionId) return std::nullopt;
    
    auto session = m_sessions->get(*sessionId);
    if (!session) return std::nullopt;
    
    return m_users->getById(std::stoll(session->userId));
}

void Handler::requireAuth(const HTTP::Request& req) {
    auto user = getAuthenticatedUser(req);
    if (!user) {
        throw AuthException(std::string("Authentication required"));
    }
}

void Handler::requireAdmin(const HTTP::Request& req) {
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        throw PermissionException(std::string("Admin access required"));
    }
}

// ============================================================================
// HTML Rendering
// ============================================================================

std::string Handler::renderLoginPage(const std::string& error) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login - OmniNet Dashboard</title>
    <link rel="stylesheet" href="/css/style.css">
    <style>
        body { display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .login-card { width: 100%; max-width: 400px; }
    </style>
</head>
<body>
    <main class="container" style="padding-top: 4rem;">
        <div class="Box login-card">
            <div class="Box-header">
                <h2 class="Box-title">
                    <svg width="24" height="24" viewBox="0 0 16 16" fill="currentColor">
                        <path d="M8 0a8 8 0 1 0 0 16A8 8 0 0 0 8 0zm3.5 9.5a.5.5 0 0 1-.5.5H5.5a.5.5 0 0 1 0-1h5.5a.5.5 0 0 1 .5.5zm-1.5 1.5h-3v-3h3v3z"/>
                    </svg>
                    OmniNet Dashboard
                </h2>
            </div>
            <div class="Box-body">)" << 
                (error.empty() ? "" : "<div class=\"flash flash-error mb-2\">" + error + "</div>") << R"(
                <form method="post" action=")" << m_config.apiPath << R"(/login">
                    <div class="form-group">
                        <label class="form-label" for="username">Username</label>
                        <input type="text" id="username" name="username" class="form-control" required autofocus>
                    </div>
                    <div class="form-group">
                        <label class="form-label" for="password">Password</label>
                        <input type="password" id="password" name="password" class="form-control" required>
                    </div>
                    <button type="submit" class="btn btn-primary btn-block">Sign in</button>
                </form>
            </div>
        </div>
    </main>
</body>
</html>)";
    return html.str();
}


std::string Handler::renderDashboardPage(const DB::UserStore::User& user) {
    (void)user;
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dashboard - OmniNet</title>
    <link rel="stylesheet" href="/css/style.css">
</head>
<body>
    <header class="header">
        <div class="container header-content">
            <h1>
                <svg width="24" height="24" viewBox="0 0 16 16" fill="currentColor">
                    <path d="M8 0a8 8 0 1 0 0 16A8 8 0 0 0 8 0zm3.5 9.5a.5.5 0 0 1-.5.5H5.5a.5.5 0 0 1 0-1h5.5a.5.5 0 0 1 .5.5zm-1.5 1.5h-3v-3h3v3z"/>
                </svg>
                OmniNet Dashboard
            </h1>
            <nav class="nav">
                <a href=")" << m_config.urlPath << R"(" class="active">Dashboard</a>
                <a href=")" << m_config.apiPath << R"(/logout" class="btn btn-secondary btn-sm">Sign out</a>
            </nav>
        </div>
    </header>

    <main class="container">
        <div class="subnav">
            <a href="#tab-overview" class="subnav-item selected" data-tab="overview">Overview</a>
            <a href="#tab-logs" class="subnav-item" data-tab="logs">Logs</a>
            <a href="#tab-users" class="subnav-item" data-tab="users">Users</a>
            <a href="#tab-settings" class="subnav-item" data-tab="settings">Settings</a>
        </div>
        
        <!-- Overview Tab -->
        <div id="tab-overview" class="tab-content active">
        
        <div class="stats-grid">
            <div class="stat-card">
                <span class="stat-value">)" << m_users->count() << R"(</span>
                <span class="stat-label">Users</span>
            </div>
            <div class="stat-card">
                <span class="stat-value">)" << m_sessions->count() << R"(</span>
                <span class="stat-label">Active Sessions</span>
            </div>
            <div class="stat-card">
                <span class="stat-value">)" << OmniNet::Admin::Handler::version() << R"(</span>
                <span class="stat-label">Version</span>
            </div>
            <div class="stat-card">
                <span class="stat-value text-success">✓</span>
                <span class="stat-label">Status</span>
            </div>
        </div>

        <div class="Box">
            <div class="Box-header">
                <h2 class="Box-title">Users</h2>
                <button class="btn btn-primary btn-sm">Add user</button>
            </div>
            <div class="Box-body" style="padding: 0;">
                <table>
                    <thead>
                        <tr>
                            <th>Username</th>
                            <th>Role</th>
                            <th>Created</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>)";
    
    for (const auto& u : m_users->list()) {
        std::string roleClass = (u.role == "admin") ? "label-primary" : "label-secondary";
        html << "<tr>";
        html << "<td><strong>" << u.username << "</strong></td>";
        html << "<td><span class=\"label " << roleClass << "\">" << u.role << "</span></td>";
        html << "<td class=\"text-muted\">-</td>";
        html << "<td><button class=\"btn btn-danger btn-sm\" onclick=\"deleteUser(" << u.id << ")\">Delete</button></td>";
        html << "</tr>";
    }
    
    if (m_users->count() == 0) {
        html << "<tr><td colspan=\"4\" class=\"text-center text-muted\">No users yet. Create your first user!</td></tr>";
    }
    
    html << R"(
                </tbody>
                </table>
            </div>
        </div>
        </div><!-- End Users Tab -->
        
        <!-- Settings Tab -->
        <div id="tab-settings" class="tab-content" style="display: none;">
            <div class="Box">
                <div class="Box-header">
                    <h2 class="Box-title">Settings</h2>
                </div>
                <div class="Box-body">
                    <p class="text-muted">Settings panel coming soon...</p>
                </div>
            </div>
        </div><!-- End Settings Tab -->
        
    </main>

    <footer class="container" style="padding: 2rem 0; border-top: 1px solid var(--color-border-default); margin-top: 2rem;">
        <p class="text-center text-muted">
            OmniNet Dashboard &middot; )" << OmniNet::Admin::Handler::version() << R"(
        </p>
    </footer>

    <script>)" << m_customJS << R"(
        // Tab switching
        document.querySelectorAll('.subnav-item').forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault();
                const tabId = this.getAttribute('data-tab');
                
                // Update nav
                document.querySelectorAll('.subnav-item').forEach(l => l.classList.remove('selected'));
                this.classList.add('selected');
                
                // Update tabs
                document.querySelectorAll('.tab-content').forEach(tab => {
                    tab.style.display = 'none';
                });
                document.getElementById('tab-' + tabId).style.display = 'block';
                
                // Load logs when tab is shown
                if (tabId === 'logs') {
                    refreshLogs();
                }
            });
        });
        
        // Log functions
        async function refreshLogs() {
            const level = document.getElementById('log-level').value;
            const tbody = document.getElementById('logs-body');
            tbody.innerHTML = '<tr><td colspan="3" class="text-center text-muted">Loading...</td></tr>';
            
            try {
                let url = ')" << m_config.apiPath << R"(/logs?limit=100';
                if (level) url += '&level=' + level;
                
                const response = await fetch(url);
                const data = await response.json();
                
                if (data.logs && data.logs.length > 0) {
                    tbody.innerHTML = data.logs.map(log => {
                        const levelClass = getLevelClass(log.level);
                        return '<tr>' +
                            '<td><span class="label ' + levelClass + '">' + log.level + '</span></td>' +
                            '<td class="text-muted" style="font-size: 12px;">' + log.timestamp + '</td>' +
                            '<td style="word-break: break-all;">' + escapeHtml(log.message) + '</td>' +
                            '</tr>';
                    }).join('');
                } else {
                    tbody.innerHTML = '<tr><td colspan="3" class="text-center text-muted">No logs found</td></tr>';
                }
            } catch (error) {
                tbody.innerHTML = '<tr><td colspan="3" class="text-center text-muted" style="color: red;">Failed to load logs</td></tr>';
            }
        }
        
        function getLevelClass(level) {
            switch(level) {
                case 'DEBUG': return 'label-secondary';
                case 'INFO': return 'label-primary';
                case 'WARN': return 'label-warning';
                case 'ERROR': return 'label-danger';
                case 'CRITICAL': return 'label-danger';
                default: return 'label-secondary';
            }
        }
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        function downloadLogs() {
            const level = document.getElementById('log-level').value;
            let url = ')" << m_config.apiPath << R"(/logs?limit=1000';
            if (level) url += '&level=' + level;
            
            window.open(url, '_blank');
        }
        
        async function deleteUser(id) {
            if (confirm('Are you sure you want to delete this user?')) {
                try {
                    const response = await fetch(')" << m_config.apiPath << R"(/users/' + id, { method: 'DELETE' });
                    if (response.ok) {
                        location.reload();
                    }
                } catch (error) {
                    alert('Failed to delete user');
                }
            }
        }
    </script>
</body>
</html>)";
    return html.str();
}

} // namespace OmniNet::Dashboard
