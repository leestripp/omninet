# OmniNet HTTP Server API

## Overview

The `OmniNet::HTTP::Server` class provides a high-performance HTTP server with support for:
- HTTP/1.1 keep-alive connections
- TLS/HTTPS encryption
- CORS (Cross-Origin Resource Sharing)
- Static file serving
- Middleware pipeline
- Request routing

## Quick Start

```cpp
#include "OmniNet.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    // Create server
    Server server;
    server.address("127.0.0.1").port(8080);
    
    // Register routes
    server.get("/api/hello", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello!"})");
    });
    
    // Start server
    server.start();
}
```

## Server Configuration

### Basic Setup

```cpp
// Using Config struct
Server::Config config;
config.address = "0.0.0.0";
config.port = 8080;
config.workerCount = 4;
config.requestTimeout = std::chrono::seconds(30);
config.keepAliveTimeout = std::chrono::seconds(5);
config.maxRequestSize = 1024 * 1024;  // 1MB

Server server(config);
```

### Using Builder Pattern

```cpp
auto server = ServerBuilder()
    .address("0.0.0.0")
    .port(8080)
    .workers(4)
    .requestTimeout(30s)
    .cors("*", "GET,POST", "Content-Type")
    .tls("cert.pem", "key.pem")
    .build();
```

## Routing

### Basic Routes

```cpp
server.get("/path", handler);           // GET requests
server.post("/path", handler);          // POST requests
server.put("/path", handler);           // PUT requests
server.delete_("/path", handler);       // DELETE requests
server.patch("/path", handler);        // PATCH requests
server.head("/path", handler);          // HEAD requests
server.options("/path", handler);      // OPTIONS requests
server.any("/path", handler);          // Any HTTP method
```

### Route Parameters

```cpp
// Match /users/123
server.get("/users/:id", [](const Request& req, Response& resp) {
    std::string id = req.param("id");
    resp.setBody("User ID: " + id);
});

// Match /posts/2024/03/15
server.get("/posts/:year/:month/:day", [](const Request& req, Response& resp) {
    std::string year = req.param("year");
    std::string month = req.param("month");
    std::string day = req.param("day");
});
```

### Wildcard Routes

```cpp
// Match /files/a/b/c.txt
server.get("/files/*", [](const Request& req, Response& resp) {
    std::string path = req.param("*");
    // path = "a/b/c.txt"
});
```

## Request Object

### Accessing Data

```cpp
server.post("/api/data", [](const Request& req, Response& resp) {
    // Path and method
    std::string path = req.path();           // "/api/data"
    std::string method = req.method();       // "POST"
    
    // Query parameters
    std::string name = req.query("name");    // ?name=John
    std::string age = req.query("age");       // ?age=25
    
    // URL parameters (from route pattern)
    std::string id = req.param("id");
    
    // Headers
    std::string auth = req.header("Authorization");
    std::string contentType = req.header("Content-Type");
    
    // Body
    std::string body = req.body();
    size_t bodySize = req.contentLength();
    
    // JSON body (parsed)
    auto json = req.json();  // Returns nlohmann::json
    std::string name = json.value("name", "");
    
    // Form data
    auto form = req.form();
    std::string email = form.value("email", "");
});
```

### Request Properties

```cpp
bool isJson = req.isJson();           // Content-Type is application/json
bool isHtml = req.isHtml();           // Content-Type is text/html
bool isSecure = req.isSecure();       // HTTPS/TLS connection
std::string ip = req.remoteIp();      // Client IP address
std::string ua = req.userAgent();     // User-Agent header
```

## Response Object

### Setting Response

```cpp
server.get("/api", [](const Request& req, Response& resp) {
    // Status code
    resp.setStatus(StatusCode::OK);           // 200
    resp.setStatus(StatusCode::CREATED);       // 201
    resp.setStatus(StatusCode::NOT_FOUND);     // 404
    resp.setStatus(StatusCode::INTERNAL_ERROR);// 500
    
    // Content type
    resp.setContentType("application/json");
    resp.setContentType("text/html");
    resp.setContentType("text/plain");
    
    // Body
    resp.setBody("Hello, World!");
    resp.setBody(R"({"key": "value"})");
    
    // JSON response (convenience)
    resp.json({{"message", "success"}});
    
    // Headers
    resp.setHeader("X-Custom-Header", "value");
    resp.setHeader("Cache-Control", "no-cache");
    
    // Cookies
    resp.setCookie("session", "abc123", {
        .maxAge = 3600,
        .path = "/",
        .httpOnly = true,
        .secure = true
    });
});
```

### Response Shortcuts

```cpp
// Send JSON (sets content type and serializes)
resp.json({{"result", 42}});

// Send HTML file
resp.sendFile("/path/to/file.html");

// Redirect
resp.redirect("/new-location");
resp.redirect("https://external.com", 302);

// Error response
resp.error(StatusCode::UNAUTHORIZED, "Authentication required");
```

## Middleware

### Global Middleware

```cpp
// Logging middleware
server.use([](const Request& req, Response& resp, auto next) {
    std::cout << "Request: " << req.method() << " " << req.path() << std::endl;
    next();
    std::cout << "Response: " << static_cast<int>(resp.status()) << std::endl;
});

// Authentication middleware
server.use([](const Request& req, Response& resp, auto next) {
    std::string token = req.header("Authorization");
    if (validateToken(token)) {
        next();  // Continue to handler
    } else {
        resp.error(StatusCode::UNAUTHORIZED, "Invalid token");
    }
});
```

### Route-Specific Middleware

```cpp
// Apply middleware to specific route
server.get("/admin/*", authMiddleware, [](const Request& req, Response& resp) {
    // Admin-only route
});
```

## CORS (Cross-Origin Resource Sharing)

### Enable CORS

```cpp
// Simple setup
server.enableCORS();

// Custom configuration
server.enableCORS(
    "https://example.com",                           // Allowed origins
    "GET, POST, PUT, DELETE",                        // Allowed methods
    "Content-Type, Authorization, X-Request-ID",   // Allowed headers
    std::chrono::seconds(86400)                     // Preflight cache duration
);

// Disable CORS
server.disableCORS();
```

### Config Options

```cpp
Server::Config config;
config.corsEnabled = true;
config.corsAllowedOrigins = "*";  // Or specific: "https://example.com"
config.corsAllowedMethods = "GET, POST, PUT, DELETE";
config.corsAllowedHeaders = "Content-Type, Authorization";
config.corsMaxAge = std::chrono::seconds(86400);
```

## TLS/HTTPS

```cpp
Server::Config config;
config.tlsEnabled = true;
config.tlsCertFile = "/path/to/certificate.pem";
config.tlsKeyFile = "/path/to/private-key.pem";
// config.tlsCaFile = "/path/to/ca-cert.pem";  // For client cert auth
config.tlsVerifyClient = false;  // true for mutual TLS

Server server(config);
```

## Static Files

```cpp
// Serve directory
server.serveStatic("/static", "/var/www/html");

// With file caching
server.serveStatic("/assets", "./public", {
    .maxAge = 3600,
    .indexFile = "index.html"
});
```

## Error Handling

### Custom Error Handler

```cpp
server.setErrorHandler([](const Request& req, Response& resp, std::exception_ptr e) {
    resp.setStatus(StatusCode::INTERNAL_ERROR);
    resp.setContentType("application/json");
    resp.setBody(R"({"error": "Something went wrong"})");
});
```

### Not Found Handler

```cpp
server.setNotFoundHandler([](const Request& req, Response& resp) {
    resp.setStatus(StatusCode::NOT_FOUND);
    resp.setContentType("text/html");
    resp.setBody("<h1>404 - Page Not Found</h1>");
});
```

## Server Lifecycle

```cpp
Server server(config);

// Blocking start
server.start();

// Non-blocking (background)
server.startAsync();
// ... do other work ...
server.wait();  // Block until stopped

// Stop gracefully
server.stop();

// Check status
if (server.isRunning()) {
    std::cout << "Server is running on port " << server.port() << std::endl;
}
```

## Statistics

```cpp
const auto& stats = server.stats();
std::cout << "Total requests: " << stats.requestsTotal.load() << std::endl;
std::cout << "Active requests: " << stats.requestsActive.load() << std::endl;
std::cout << "Successful: " << stats.requestsSuccess.load() << std::endl;
std::cout << "Errors: " << stats.requestsError.load() << std::endl;
std::cout << "Bytes received: " << stats.bytesReceived.load() << std::endl;
std::cout << "Bytes sent: " << stats.bytesSent.load() << std::endl;
```

## Complete Example

```cpp
#include "OmniNet.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server server({
        .port = 8080,
        .workerCount = 4,
        .corsEnabled = true,
        .corsAllowedOrigins = "*",
        .requestTimeout = 30s,
        .logRequests = true
    });
    
    // Health check
    server.get("/health", [](const Request& req, Response& resp) {
        resp.json({{"status", "healthy"}});
    });
    
    // API with parameters
    server.get("/users/:id", [](const Request& req, Response& resp) {
        std::string id = req.param("id");
        resp.json({
            {"id", id},
            {"name", "User " + id}
        });
    });
    
    // POST endpoint
    server.post("/users", [](const Request& req, Response& resp) {
        auto body = req.json();
        std::string name = body.value("name", "");
        
        // Create user...
        
        resp.setStatus(StatusCode::CREATED);
        resp.json({{"id", "123"}, {"name", name}});
    });
    
    // Start server
    std::cout << "Server starting on port 8080..." << std::endl;
    server.start();
    
    return 0;
}
```

## See Also

- [WebSocket API](websocket.md) - WebSocket connections
- [Database API](database.md) - SQLite database access
- [Config System](../config.md) - Configuration options