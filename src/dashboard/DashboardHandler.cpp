// OmniNet - Dashboard Handler Implementation
#include "DashboardHandler.h"

namespace OmniNet::Dashboard {

// ============================================================================
// Handler
// ============================================================================

Handler::Handler(const Config& config)
    : m_config(config) {
    
    // Open database
    std::string dbPath = config.dbPath.empty() ? 
        Config::resolve("dashboard.db") : config.dbPath;
    
    m_db = std::make_unique<DB::Database>();
    m_db->open(dbPath);
    
    m_sessions = std::make_unique<SessionStore>(*m_db);
    m_sessions->initialize();
    
    m_users = std::make_unique<UserStore>(*m_db);
    m_users->initialize();
    
    m_stats = std::make_unique<StatsStore>();
    m_rateLimiter = std::make_unique<RateLimiter>(
        config.maxLoginAttempts,
        std::chrono::minutes(15)
    );
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
}

bool Handler::handleRequest(const HTTP::Request& req, HTTP::Response& resp) {
    // Check IP
    auto ip = req.remoteAddress();
    if (!m_config.allowedIpPrefixes.empty() && !isAllowedIp(ip)) {
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

SessionStore& Handler::sessions() {
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
    auto ip = req.remoteAddress();
    
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
    resp = HTTP::Response::notImplemented();
}

void Handler::handleGetStats(const HTTP::Request& req, HTTP::Response& resp) {
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody("{}");
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
            return header->substr(7);
        }
    }
    return header ? std::optional<std::string>(*header) : std::nullopt;
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
        throw AuthException("Authentication required");
    }
}

void Handler::requireAdmin(const HTTP::Request& req) {
    auto user = getAuthenticatedUser(req);
    if (!user || user->role != "admin") {
        throw PermissionException("Admin access required");
    }
}

// ============================================================================
// HTML Rendering
// ============================================================================

std::string Handler::renderLoginPage(const std::string& error) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <title>Login - OmniNet Dashboard</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .login-box { background: #16213e; padding: 40px; border-radius: 8px; box-shadow: 0 4px 20px rgba(0,0,0,0.5); width: 320px; }
        h1 { margin: 0 0 30px 0; font-size: 24px; text-align: center; }
        input { width: 100%; padding: 12px; margin: 10px 0; border: none; border-radius: 4px; box-sizing: border-box; background: #0f3460; color: #fff; }
        button { width: 100%; padding: 12px; background: #e94560; color: #fff; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 10px; }
        button:hover { background: #d63e56; }
        .error { color: #e94560; margin-bottom: 20px; text-align: center; }
    </style>
</head>
<body>
    <div class="login-box">
        <h1>OmniNet Dashboard</h1>
        )" << (error.empty() ? "" : "<div class='error'>" + error + "</div>") << R"(
        <form method="post" action=")" << m_config.apiPath << R"(/login">
            <input type="text" name="username" placeholder="Username" required>
            <input type="password" name="password" placeholder="Password" required>
            <button type="submit">Login</button>
        </form>
    </div>
</body>
</html>)";
    return html.str();
}

std::string Handler::renderDashboardPage(const DB::UserStore::User& user) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <title>OmniNet Dashboard</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; margin: 0; }
        .header { background: #16213e; padding: 20px 40px; display: flex; justify-content: space-between; align-items: center; }
        .header h1 { margin: 0; font-size: 24px; }
        .logout { background: #e94560; color: #fff; padding: 8px 16px; text-decoration: none; border-radius: 4px; }
        .container { padding: 40px; max-width: 1200px; margin: 0 auto; }
        .card { background: #16213e; border-radius: 8px; padding: 24px; margin-bottom: 24px; }
        .card h2 { margin: 0 0 20px 0; font-size: 18px; color: #e94560; }
        table { width: 100%; border-collapse: collapse; }
        th, td { text-align: left; padding: 12px; border-bottom: 1px solid #0f3460; }
        th { color: #888; }
        .btn { padding: 8px 16px; border-radius: 4px; cursor: pointer; border: none; }
        .btn-danger { background: #e94560; color: #fff; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; }
        .stat { background: #0f3460; padding: 20px; border-radius: 8px; text-align: center; }
        .stat-value { font-size: 36px; font-weight: bold; color: #e94560; }
        .stat-label { color: #888; margin-top: 8px; }
    </style>
)" << m_customCSS << R"(
</head>
<body>
    <div class="header">
        <h1>OmniNet Dashboard</h1>
        <a href=")" << m_config.apiPath << R"(/logout" class="logout">Logout</a>
    </div>
    <div class="container">
        <div class="stats">
            <div class="stat">
                <div class="stat-value">)" << m_users->count() << R"(</div>
                <div class="stat-label">Users</div>
            </div>
            <div class="stat">
                <div class="stat-value">)" << m_sessions->count() << R"(</div>
                <div class="stat-label">Active Sessions</div>
            </div>
        </div>
        <div class="card">
            <h2>Users</h2>
            <table>
                <thead><tr><th>Username</th><th>Role</th><th>Actions</th></tr></thead>
                <tbody>)";
    
    for (const auto& u : m_users->list()) {
        html << "<tr><td>" << u.username << "</td><td>" << u.role << "</td>";
        html << R"(<td><button class="btn btn-danger" onclick="deleteUser()" data-id=")" << u.id << R"(">Delete</button></td></tr>)";
    }
    
    html << R"(</tbody></table>
        </div>
    </div>
    <script>)" << m_customJS << R"(
        async function deleteUser(id) {
            if (confirm('Delete user?')) {
                await fetch('/api/users/' + id, { method: 'DELETE' });
                location.reload();
            }
        }
    </script>
</body>
</html>)";
    return html.str();
}

} // namespace OmniNet::Dashboard