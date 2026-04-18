# OmniNet HTTP Server API

## Overview

The `OmniNet::HTTP::Server` class provides a high-performance HTTP server with support for:
- HTTP/1.1 keep-alive connections
- TLS/HTTPS encryption
- CORS (Cross-Origin Resource Sharing)
- Static file serving
- Request routing with parameters
- Admin endpoints (`/omninet/*`)
- Request/response statistics

## Quick Start

```cpp
#include "OmniNet.h"
#include "http/StaticHandler.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    // Create server with config
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8080;
    config.workerCount = 4;
    
    Server server(config);
    
    // Register routes
    server.get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello!"})");
    });
    
    // Mount admin handler for /omninet/* endpoints
    server.adminHandler().mount(server);
    
    server.start();
}
```

## Server Configuration

### Basic Setup

```cpp
Server::Config config;
config.address = "0.0.0.0";
config.port = 8080;
config.workerCount = 4;  // 0 = auto-detect
config.requestTimeout = std::chrono::seconds(30);
config.keepAliveTimeout = std::chrono::seconds(5);
config.maxRequestSize = 1024 * 1024;  // 1MB

Server server(config);
```

### TLS/HTTPS Setup

```cpp
Server::Config config;
config.port = 8443;
config.tlsEnabled = true;
config.tlsCertFile = "/path/to/cert.pem";
config.tlsKeyFile = "/path/to/key.pem";
config.tlsVerifyClient = false;  // Don't require client cert

Server server(config);
```

### CORS Setup

```cpp
Server::Config config;
config.corsEnabled = true;
config.corsAllowedOrigins = "*";  // Or specific origin
config.corsAllowedMethods = "GET,POST,PUT,DELETE";
config.corsAllowedHeaders = "Content-Type,Authorization";
config.corsMaxAge = std::chrono::seconds(86400);

Server server(config);
```

### Using Builder Pattern (Alternative)

```cpp
auto server = ServerBuilder()
    .address("0.0.0.0")
    .port(8080)
    .workers(4)
    .requestTimeout(30s)
    .cors("*", "GET,POST", "Content-Type")
    .build();
```

## Routing

### Basic Routes

```cpp
server.get("/path", handler);           // GET requests
server.post("/path", handler);          // POST requests
server.put("/path", handler);          // PUT requests
server.delete_("/path", handler);      // DELETE requests
server.patch("/path", handler);        // PATCH requests
server.head("/path", handler);         // HEAD requests
server.options("/path", handler);       // OPTIONS requests
server.any("/path", handler);          // Any HTTP method
```

### Route Parameters

```cpp
// Match /users/123
server.get("/users/:id", [](const Request& req, Response& resp) {
    std::string id(req.pathParam(0));  // First parameter
    resp.setBody("User ID: " + id);
});

// Match /posts/2024/03/15
server.get("/posts/:year/:month/:day", [](const Request& req, Response& resp) {
    std::string year(req.pathParam(0));
    std::string month(req.pathParam(1));
    std::string day(req.pathParam(2));
    resp.setBody("Date: " + year + "-" + month + "-" + day);
});

// Named parameters
server.get("/users/:id", [](const Request& req, Response& resp) {
    std::string id(req.pathParam("id"));
    resp.setBody("User: " + id);
});
```

### Wildcards

```cpp
// Match /files/*
server.get("/files/*", [](const Request& req, Response& resp) {
    std::string path(req.path());
    resp.setBody("Requested: " + path);
});

// Serve static files
StaticHandler staticHandler("./static");
server.get("/*", [&staticHandler](const Request& req, Response& resp) {
    staticHandler.handle(req, resp);
});
```

### Route Priority

Routes are matched in order of registration. First match wins:

```cpp
// Register specific routes FIRST
server.get("/api/users", handleUsers);
server.get("/api/posts", handlePosts);

// Then catch-all
server.get("/*", handleStatic);
```

## Admin Endpoints

The admin handler provides built-in endpoints under `/omninet/*`:

### Enable Admin Handler

```cpp
server.adminHandler().mount(server);
```

### Available Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/omninet` | GET | Admin dashboard HTML |
| `/omninet/health` | GET | Health check (JSON) |
| `/omninet/stats` | GET | Server statistics (JSON) |

### Health Endpoint Response

```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Stats Endpoint Response

```json
{
  "uptime": 3600,
  "requestsTotal": 10000,
  "requestsSuccess": 9500,
  "requestsError": 500,
  "bytesReceived": 1048576,
  "bytesSent": 2097152,
  "connectionsTotal": 500,
  "connectionsActive": 10
}
```

## Request Object

### Accessing Data

```cpp
server.get("/api/example", [](const Request& req, Response& resp) {
    // Path
    std::string path(req.path());           // "/api/example"
    
    // Query parameters
    auto& query = req.query();
    auto name = query.get("name");          // ?name=value
    
    // Headers
    auto contentType = req.contentType();   // "application/json"
    auto auth = req.header("Authorization");
    
    // Body
    std::string body = req.bodyAsString();
    
    // Path parameters (for /users/:id)
    std::string id(req.pathParam(0));
    std::string name(req.pathParam("name"));
});
```

### Request Properties

```cpp
// Method
req.method();              // GET, POST, etc.

 // Path
req.path();                 // "/api/users/123"
req.pathParam(0);          // "123" (first param)
req.pathParam("id");       // "123" (named param)

// Query string
req.queryString();         // "page=1&limit=10"
req.query();               // QueryParams object

// Headers
req.header("Content-Type");
req.hasHeader("Authorization");

// Body
req.hasBody();             // true if body present
req.contentLength();       // body size in bytes
req.contentType();         // MIME type

// Version
req.version();             // HTTP::Version{1, 1}
```

## Response Object

### Basic Response

```cpp
resp.setStatus(StatusCode::OK);
resp.setContentType("application/json");
resp.setBody(R"({"key": "value"})");
```

### Response Shortcuts

```cpp
// JSON response
resp.setStatus(StatusCode::OK);
resp.setContentType("application/json");
resp.setBody("{\"message\": \"success\"}");

// HTML response
resp.setStatus(StatusCode::OK);
resp.setContentType("text/html");
resp.setBody("<html><body>Hello</body></html>");

// Empty response
resp.setStatus(StatusCode::NoContent);
```

### Setting Headers

```cpp
resp.setHeader("X-Custom-Header", "value");
resp.setHeader("Cache-Control", "no-cache");

// Remove header
resp.removeHeader("X-Old-Header");
```

### CORS Headers

When CORS is enabled, these headers are automatically added:
- `Access-Control-Allow-Origin`
- `Access-Control-Allow-Methods`
- `Access-Control-Allow-Headers`
- `Access-Control-Max-Age`

### Error Responses

```cpp
// 400 Bad Request
resp.setStatus(StatusCode::BadRequest);
resp.setBody("{\"error\": \"Invalid request\"}");

// 404 Not Found
resp.setStatus(StatusCode::NotFound);
resp.setBody("Not found");

// 500 Internal Server Error
resp.setStatus(StatusCode::InternalServerError);
resp.setBody("Server error");
```

## Static File Serving

### Basic Setup

```cpp
#include "http/StaticHandler.h"

StaticHandler staticHandler("./static");
staticHandler.setDefaultFile("index.html");

// Register catch-all route
server.get("/*", [&staticHandler](const Request& req, Response& resp) {
    // Skip admin paths
    std::string path(req.path());
    if (path.rfind("/omninet/", 0) == 0) {
        resp = Response::notFound();
        return;
    }
    staticHandler.handle(req, resp);
});
```

### Configuration

```cpp
StaticHandler handler("./static");
handler.setDefaultFile("index.html");      // Default file for directories
handler.setIndexFiles({"index.html", "index.htm"});
handler.enableDirectoryListing(false);    // Disable directory browsing
handler.addAlias("/assets", "/var/www/assets");  // Path aliasing
```

## Error Handling

### Custom 404 Handler

```cpp
server.setNotFoundHandler([](const Request& req, Response& resp) {
    (void)req;
    resp.setStatus(StatusCode::NotFound);
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>404 Not Found</h1></body></html>");
});
```

### Custom Error Handler

```cpp
server.setErrorHandler([](const Request& req, Response& resp) {
    resp.setStatus(StatusCode::InternalServerError);
    resp.setContentType("text/plain");
    resp.setBody("Internal Server Error");
});
```

## Middleware

### Global Middleware

```cpp
server.use([](const Request& req, Response& resp, auto next) {
    // Before handler
    auto start = std::chrono::steady_clock::now();
    
    next();  // Call handler
    
    // After handler
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();
    std::cout << "Request took " << elapsed << "ms" << std::endl;
});
```

## Statistics

### Access Server Stats

```cpp
const auto& stats = server.stats();
std::cout << "Total requests: " << stats.requestsTotal.load() << std::endl;
std::cout << "Active connections: " << stats.connectionsActive.load() << std::endl;
std::cout << "Bytes sent: " << stats.bytesSent.load() << std::endl;
```

### Admin Stats Endpoint

```cpp
// GET /omninet/stats
auto adminStats = server.adminHandler().getStats();
std::cout << "Uptime: " << adminStats.uptime.count() << "s" << std::endl;
```

## Full Example

```cpp
#include "OmniNet.h"
#include "http/StaticHandler.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    // Configure server
    Server::Config config;
    config.address = "0.0.0.0";
    config.port = 8080;
    config.workerCount = 0;  // Auto-detect
    config.corsEnabled = true;
    
    Server server(config);
    
    // API routes
    server.get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    server.get("/api/time", [](const Request& req, Response& resp) {
        (void)req;
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        resp.setContentType("application/json");
        resp.setBody("{\"timestamp\": " + std::to_string(timestamp) + "}");
    });
    
    server.post("/api/echo", [](const Request& req, Response& resp) {
        resp.setContentType("application/json");
        resp.setBody("{\"echo\": \"" + req.bodyAsString() + "\"}");
    });
    
    // Mount admin handler
    server.adminHandler().mount(server);
    
    // Static files
    StaticHandler staticHandler("./static");
    staticHandler.setDefaultFile("index.html");
    
    server.get("/*", [&staticHandler](const Request& req, Response& resp) {
        std::string path(req.path());
        if (path.rfind("/omninet/", 0) == 0) {
            resp = Response::notFound();
            return;
        }
        staticHandler.handle(req, resp);
    });
    
    std::cout << "Server starting on port " << config.port << std::endl;
    server.start();
    
    OmniNet::cleanup();
    return 0;
}
```

## Status Codes

Available `StatusCode` values:

```cpp
// 2xx Success
StatusCode::OK = 200
StatusCode::Created = 201
StatusCode::NoContent = 204

// 3xx Redirection
StatusCode::Found = 302
StatusCode::NotModified = 304

// 4xx Client Error
StatusCode::BadRequest = 400
StatusCode::Unauthorized = 401
StatusCode::Forbidden = 403
StatusCode::NotFound = 404
StatusCode::MethodNotAllowed = 405
StatusCode::RequestTimeout = 408
StatusCode::TooManyRequests = 429

// 5xx Server Error
StatusCode::InternalServerError = 500
StatusCode::NotImplemented = 501
StatusCode::BadGateway = 502
StatusCode::ServiceUnavailable = 503
StatusCode::GatewayTimeout = 504
```