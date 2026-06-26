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

### TLS Client Connections (v0.7.2+)

For client-side TLS (e.g., making outbound HTTPS requests), use
`TLSContext` to create sockets. The recommended pattern is:

```cpp
#include "net/TLSSocket.h"
using namespace OmniNet;

// Create a client context — loads system CA bundle,
// sets SSL_VERIFY_PEER by default (secure out of the box).
auto ctx = TLSContext::createClient();

// Create a socket that uses the shared context.
auto sock = ctx->createSocket();

// (Optional) Advertise HTTP/1.1 over ALPN so servers
// like Cloudflare/GitHub don't default to h2 binary.
sock->setALPNProtocols({"h2", "http/1.1"});

// Connect + do TLS handshake.
sock->connect("api.github.com", 443);

// Read the response — readLine() now works correctly on TLS.
auto statusLine = sock->readLine();
// "HTTP/1.1 200 OK"

// Get the negotiated ALPN protocol.
auto alpn = sock->getALPNProtocol();
// "http/1.1" (or "h2" if the server picked h2)
```

#### TLSContext Factory Methods

| Method | Description |
|--------|-------------|
| `TLSContext::createServer(certFile, keyFile)` | Creates a server context. Loads cert + key, returns shared_ptr. |
| `TLSContext::createClient()` | Creates a client context. Loads system CA bundle, sets `SSL_VERIFY_PEER` by default. Returns shared_ptr. |
| `TLSContext::createSocket()` | Creates a `TLSSocket` that shares this context (uses `shared_from_this()`). |

#### Security Defaults (v0.7.2+)

`TLSContext::createClient()` is secure by default:
- System CA bundle is loaded via `SSL_CTX_set_default_verify_paths()`
- Verify mode is `SSL_VERIFY_PEER` (rejects forged certificates)
- Calling `setVerifyMode(TLSVerify::NONE)` is required to opt out

To customize verification further (e.g., add a custom CA file):

```cpp
auto ctx = TLSContext::createClient();
SSL_CTX_load_verify_locations(
    ctx->context(),
    "/path/to/my-ca-bundle.crt",
    nullptr  // no dir
);
auto sock = ctx->createSocket();
sock->connect("internal.example.com", 443);
```

#### Sharing Contexts (Performance)

Creating one `TLSContext` and sharing it across many sockets saves
memory (one `SSL_CTX` per socket → one for N sockets) and load time
(CA bundle loaded once, not per-socket):

```cpp
auto ctx = TLSContext::createClient();

std::vector<std::unique_ptr<TLSSocket>> sockets;
for (auto& host : hosts) {
    auto s = ctx->createSocket();
    s->connect(host, 443);
    sockets.push_back(std::move(s));
}
```

#### ALPN (v0.7.2+)

ALPN (Application-Layer Protocol Negotiation) lets the client tell
the server which protocols it supports, so the server picks h2 vs
http/1.1. Without ALPN, servers default to h2 (binary wire format),
which looks like garbage (`PRI * HTTP/2.0`) to text-line HTTP/1.1
parsers.

```cpp
TLSSocket sock;  // OR auto sock = ctx->createSocket();
sock.setALPNProtocols({"h2", "http/1.1"});
sock.connect("api.github.com", 443);
```

The wire format is built internally (RFC 7301 length-prefixed list)
and passed to `SSL_CTX_set_alpn_protos()`. After handshake, call
`getALPNProtocol()` to see which protocol was selected.

### CORS Setup

```cpp
Server::Config config;
config.corsEnabled = true;
config.corsAllowedOrigins = "*";  // Or comma-separated allowlist
config.corsAllowedMethods = "GET,POST,PUT,DELETE";
config.corsAllowedHeaders = "Content-Type,Authorization";
config.corsMaxAge = std::chrono::seconds(86400);

Server server(config);
```

#### Origin Allowlist Behavior (v0.7.1+)

The `corsAllowedOrigins` field is parsed at server startup into an
allowlist. The behavior depends on its contents:

| Config value | Behavior |
|--------------|----------|
| `"*"` | Allow any origin. Emits `Access-Control-Allow-Origin: *` for all requests. |
| `"https://a.com,https://b.com"` | Parsed into allowlist `{"https://a.com", "https://b.com"}`. Per request: looks up the `Origin` header; if found, echoes it back + adds `Vary: Origin` and `Access-Control-Allow-Credentials: true`. If not found, omits the header (browser blocks response). |
| `""` (empty) | CORS still enabled but no origins allowed — all cross-origin requests are blocked. |

This matches the [CORS spec](https://fetch.spec.whatwg.org/#http-cors-protocol)
which requires `Access-Control-Allow-Origin` to be a single origin or
`"*"` — a CSV is rejected by every browser.

**Note:** In v0.7.0 and earlier, the config string was emitted
verbatim as the header value, which was invalid. Always upgrade
to v0.7.2+ for correct CORS behavior.
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

### Route Priority & Registration Order

**Important:** OmniNet uses **first-match-wins** routing. Routes are matched in the order they are registered, and the first matching route handles the request.

#### Registration Order Matters

```cpp
// ✅ CORRECT: Specific routes BEFORE catch-all
server.get("/api/v1/accounts", handleAccounts);  // Registered first - will match /api/v1/accounts
server.get("/api/v1/users", handleUsers);       // Registered second
server.get("/*", handleStatic);                 // Registered last - fallback only

// ❌ WRONG: Catch-all BEFORE specific routes
server.get("/*", handleStatic);                 // Registered first - catches ALL requests!
server.get("/api/v1/accounts", handleAccounts);  // Never matches because /* catches first
```

#### Common Pitfalls

**Pitfall 1: Catch-all shadowing specific routes**
```cpp
// This will NEVER match /api/v1/accounts
server.get("/*", staticHandler);
server.get("/api/v1/accounts", accountsHandler);

// Fix: Register specific routes first
server.get("/api/v1/accounts", accountsHandler);
server.get("/*", staticHandler);
```

**Pitfall 2: Generic route before specific route**
```cpp
// This will NEVER match /api/v1/users
server.get("/api/*", genericHandler);
server.get("/api/v1/users", specificHandler);

// Fix: More specific routes first
server.get("/api/v1/users", specificHandler);
server.get("/api/*", genericHandler);
```

**Pitfall 3: Route groups with conflicting patterns**
```cpp
// If using groups, ensure group routes don't conflict
router.group("/api/v1", [](Router& r) {
    r.get("/accounts", handler);  // Will be /api/v1/accounts
});
// Then register specific override
router.get("/api/v1/accounts", overrideHandler);  // Registered after group
```

#### Best Practices

1. **Register specific routes first** - Routes with more specific patterns should be registered before generic ones
2. **Register catch-all routes last** - The `/*` or wildcard patterns should always be registered after specific routes
3. **Use parameter routes for dynamic paths** - Use `:param` syntax instead of wildcards when possible
4. **Test route matching** - Use `router.match()` to verify your routes are registered in the correct order

#### Verifying Route Order

You can inspect registered routes to verify order:

```cpp
auto routes = server.router().routes();
for (size_t i = 0; i < routes.size(); i++) {
    std::cout << i << ": " << routes[i].pattern << std::endl;
}
```

#### Route Patterns

| Pattern | Matches | Example |
|---------|---------|---------|
| `/exact` | Exact path only | `/api/users` matches only `/api/users` |
| `/:param` | Single path segment | `/:id` matches `/123` but not `/123/456` |
| `/path/*` | Path and all below | `/api/*` matches `/api/v1/accounts` |
| `/path/:id` | Path with param | `/users/:id` matches `/users/123` |

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

When CORS is enabled, the response headers depend on the request's
`Origin` header and the configured allowlist:

| Configured origins | Request `Origin` | Headers added |
|-------------------|------------------|---------------|
| `"*"` | (any) | `Access-Control-Allow-Origin: *` |
| `"https://a.com,https://b.com"` | `https://a.com` | `Access-Control-Allow-Origin: https://a.com`<br>`Vary: Origin`<br>`Access-Control-Allow-Credentials: true` |
| `"https://a.com,https://b.com"` | `https://evil.com` | *(none — browser blocks the response)* |
| `"https://a.com,https://b.com"` | *(absent)* | *(none — same-origin request, no CORS needed)* |

Always added (regardless of Origin):
- `Access-Control-Allow-Methods`
- `Access-Control-Allow-Headers`
- `Access-Control-Max-Age`

#### Reading the response from JS

```js
// Browser behavior:
//  - Origin matches allowlist: response is delivered to JS
//  - Origin doesn't match: browser blocks, JS sees a network error
//  - Configured "*" with credentials: browser blocks (spec violation),
//    use explicit allowlist instead
fetch('https://api.example.com/data', { credentials: 'include' })
```

The `Vary: Origin` header tells CDNs/proxies not to cache one
origin's response and serve it to a different origin.

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