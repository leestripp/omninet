# OmniNet Dashboard API

## Overview

The OmniNet Dashboard provides:
- Web-based admin interface at `/omninet`
- REST API at `/omninet/api/*`
- User authentication and session management
- User management (CRUD)
- System statistics
- Log viewer

## Quick Start

```cpp
#include "OmniNet.h"

using namespace OmniNet;

int main() {
    OmniNet::init();
    
    // Configure dashboard
    Dashboard::Handler::Config config;
    config.urlPath = "/omninet";
    config.apiPath = "/omninet/api";
    config.sessionTtlSeconds = 3600;
    config.maxLoginAttempts = 5;
    
    Dashboard::Handler dashboard(config);
    
    // Mount on server
    HTTP::Server::Config serverConfig;
    serverConfig.port = 8080;
    HTTP::Server server(serverConfig);
    
    dashboard.mount(server);
    
    server.start();
}
```

## Dashboard Handler

### Configuration

```cpp
Dashboard::Handler::Config config;
config.urlPath = "/omninet";           // Dashboard URL (default)
config.apiPath = "/omninet/api";       // API URL prefix (default)
config.dbPath = "";                    // Empty = default (~/.omninet/dashboard.db)
config.sessionTtlSeconds = 3600;      // Session lifetime (default: 1 hour)
config.maxLoginAttempts = 5;          // Max login failures before lockout
config.enabled = true;                 // Enable/disable dashboard
config.allowRemoteAccess = false;     // Restrict to localhost
config.allowedIpPrefixes = {"127.", "::1"};  // Allowed IP prefixes
```

### Mounting

```cpp
Dashboard::Handler dashboard(config);
dashboard.mount(server);

// The dashboard registers these routes:
// GET  /omninet                    - Dashboard HTML
// POST /omninet/api/login          - Login
// POST /omninet/api/logout         - Logout
// GET  /omninet/api/users          - List users
// POST /omninet/api/users          - Create user
// DELETE /omninet/api/users/:id    - Delete user
// GET  /omninet/api/stats          - System stats
// GET  /omninet/api/logs           - Recent logs
```

## User Management

### User Structure

```cpp
struct User {
    int64_t id;
    std::string username;
    std::string passwordHash;  // bcrypt hash
    std::string role;          // "admin", "user", etc.
    std::string email;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastLogin;
    int loginCount;
};
```

### Create User

```cpp
dashboard.users().create(username, password, email, role);
```

### List Users

```cpp
for (const auto& user : dashboard.users().list()) {
    std::cout << user.username << " (" << user.role << ")" << std::endl;
}
```

### Find User

```cpp
auto user = dashboard.users().findByUsername("admin");
if (user) {
    std::cout << "Found: " << user->email << std::endl;
}
```

### Verify Password

```cpp
auto user = dashboard.users().verify(username, password);
if (user) {
    // Valid credentials
}
```

### Delete User

```cpp
dashboard.users().remove(userId);
```

### Update Password

```cpp
dashboard.users().updatePassword(userId, newPassword);
```

## Session Management

### Login

```cpp
// Via API
auto sessionId = dashboard.sessions().create(userId);

// Sessions expire after sessionTtlSeconds
// Store sessionId in cookie or header for subsequent requests
```

### Session Validation

```cpp
// Check if session is valid
auto session = dashboard.sessions().get(sessionId);
if (session) {
    // Valid session
    int64_t userId = session->userId;
}
```

### Logout

```cpp
dashboard.sessions().remove(sessionId);
```

### List Active Sessions

```cpp
for (const auto& session : dashboard.sessions().list()) {
    std::cout << "User " << session.userId << ", created: " 
              << std::chrono::system_clock::to_time_t(session.createdAt) << std::endl;
}
```

## API Endpoints

### POST /omninet/api/login

**Request:**
```json
{
    "username": "admin",
    "password": "secret"
}
```

**Response:**
```json
{
    "success": true,
    "user": {
        "id": 1,
        "username": "admin",
        "role": "admin"
    },
    "sessionId": "abc123..."
}
```

**Error Response:**
```json
{
    "success": false,
    "error": "Invalid credentials"
}
```

### POST /omninet/api/logout

**Request:**
```json
{
    "sessionId": "abc123..."
}
```

**Response:**
```json
{
    "success": true
}
```

### GET /omninet/api/users

**Headers:**
```
X-Session-ID: abc123...
```

**Response:**
```json
{
    "users": [
        {"id": 1, "username": "admin", "role": "admin", "email": "admin@example.com"},
        {"id": 2, "username": "user1", "role": "user", "email": "user@example.com"}
    ]
}
```

### POST /omninet/api/users

**Headers:**
```
X-Session-ID: abc123...
```

**Request:**
```json
{
    "username": "newuser",
    "password": "password123",
    "email": "newuser@example.com",
    "role": "user"
}
```

**Response:**
```json
{
    "success": true,
    "user": {
        "id": 3,
        "username": "newuser",
        "role": "user"
    }
}
```

### DELETE /omninet/api/users/:id

**Headers:**
```
X-Session-ID: abc123...
```

**Response:**
```json
{
    "success": true
}
```

### GET /omninet/api/stats

**Response:**
```json
{
    "users": 5,
    "activeSessions": 2,
    "version": "0.1.0"
}
```

### GET /omninet/api/logs

**Query Parameters:**
- `limit` - Number of log entries (default: 100, max: 1000)
- `level` - Filter by level (DEBUG, INFO, WARN, ERROR, CRITICAL)
- `pattern` - Filter by message pattern

**Response:**
```json
{
    "logs": [
        {
            "level": "INFO",
            "timestamp": "2024-01-01 12:00:00.123",
            "threadId": "139978714599744",
            "message": "Server started"
        }
    ],
    "count": 1
}
```

## Dashboard UI

The dashboard HTML is embedded in the handler. Access it at:

```
http://localhost:8080/omninet
```

### Features

- **Login Page** - Secure authentication
- **Overview Tab** - System stats and quick actions
- **Logs Tab** - View recent logs with filtering
- **Users Tab** - User management (list, create, delete)
- **Settings Tab** - Configuration options

### Log Viewer

The Logs tab provides:
- Table view with Level, Timestamp, Message columns
- Level filter dropdown
- Pattern search
- Refresh button
- Download button (opens API in new tab)

## Access Control

### IP Restrictions

```cpp
Dashboard::Handler::Config config;
config.allowedIpPrefixes = {"127.", "::1"};  // localhost only

// Add more prefixes
config.allowedIpPrefixes.push_back("192.168.");
config.allowedIpPrefixes.push_back("10.");
```

### Role-Based Access

Check user role after authentication:

```cpp
// In your handler
auto user = dashboard.getAuthenticatedUser(request);
if (user && user->role == "admin") {
    // Allow admin action
} else {
    // Deny
}
```

## Rate Limiting

The dashboard includes rate limiting for login attempts:

```cpp
Dashboard::Handler::Config config;
config.maxLoginAttempts = 5;  // Max failed attempts
// Lockout duration set in RateLimiter constructor (default: 15 min)
```

### Check if Blocked

```cpp
if (dashboard.rateLimiter()->isBlocked(ipAddress)) {
    // Reject request
    resp.setStatus(StatusCode::TooManyRequests);
    resp.setBody("Too many failed attempts. Try again later.");
}
```

### Record Login Attempt

```cpp
dashboard.rateLimiter()->recordFailure(ipAddress);  // On failed login
dashboard.rateLimiter()->recordSuccess(ipAddress);   // On successful login
```

## Statistics Store

The dashboard includes a simple key-value statistics store:

```cpp
StatsStore& stats = dashboard.stats();

// Set values
stats.set("requests_total", 1000);
stats.set("bytes_sent", 50000);
stats.set("config_json", someJsonString);

// Increment
stats.increment("requests_total");
stats.increment("errors", 5);

// Get values
auto total = stats.getInt("requests_total");
auto config = stats.get("config_json");

// List all keys
for (const auto& key : stats.keys()) {
    std::cout << key << std::endl;
}
```

## Full Example

```cpp
#include "OmniNet.h"

using namespace OmniNet;

int main() {
    OmniNet::init();
    
    // Create dashboard
    Dashboard::Handler::Config dashConfig;
    dashConfig.urlPath = "/admin";
    dashConfig.apiPath = "/admin/api";
    dashConfig.sessionTtlSeconds = 7200;  // 2 hours
    dashConfig.maxLoginAttempts = 3;
    dashConfig.allowedIpPrefixes = {"127.", "192.168."};
    
    Dashboard::Handler dashboard(dashConfig);
    
    // Create server
    HTTP::Server::Config serverConfig;
    serverConfig.port = 8080;
    HTTP::Server server(serverConfig);
    
    // Mount dashboard
    dashboard.mount(server);
    
    // Your API routes
    server.get("/api/data", [](const HTTP::Request& req, HTTP::Response& resp) {
        // Validate session
        auto user = dashboard.getAuthenticatedUser(req);
        if (!user) {
            resp.setStatus(HTTP::StatusCode::Unauthorized);
            resp.setBody("{\"error\": \"Login required\"}");
            return;
        }
        
        resp.setContentType("application/json");
        resp.setBody("{\"data\": \"secret\"}");
    });
    
    // Start server
    server.start();
    
    OmniNet::cleanup();
    return 0;
}
```

## Data Storage

```
~/.omninet/
├── dashboard.db      # SQLite database
│   ├── sessions     # Session storage
│   └── users        # User storage
└── config.json      # Dashboard configuration
```