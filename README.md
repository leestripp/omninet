![OmniNet Banner](docs/resources/banner.png)

# OmniNet

A modern, high-performance C++ HTTP/WebSocket server library with TLS support, session management, admin dashboard, and more.

## Features

- **HTTP Server** - Full HTTP/1.1 server implementation with keep-alive support
- **WebSocket** - Full WebSocket protocol support (RFC 6455)
- **TLS/HTTPS** - Secure connections using OpenSSL 3.0
- **Static File Serving** - Built-in static file handler with MIME types
- **Session Management** - SQLite-backed session store with secure tokens
- **API Keys** - Secure API key management with scopes and expiration
- **User Authentication** - Password hashing with bcrypt
- **Rate Limiting** - Request rate limiting per IP
- **Admin Dashboard** - Web-based monitoring at `/omninet`
- **Log Viewer** - View logs in dashboard with filtering
- **SSE (Server-Sent Events)** - Real-time server push
- **Intrusion Detection** - Basic security monitoring
- **Log Rotation** - Automatic rotation based on file size
- **CLI Tools** - Admin tools for users, certs, logs, etc.
- **Public Test Framework** - Header-only test utilities for users

## Requirements

- C++23 compatible compiler (GCC 13+, Clang 17+, MSVC 19.36+)
- CMake 3.16+
- OpenSSL 1.1.1+ (3.x recommended)
- SQLite3

### C++23 Features Used

OmniNet uses modern C++23 features including:
- `std::expected` (planned for error handling)
- `std::move_only_function` (planned for callbacks)
- `std::flat_map` (planned for header storage)
- Deducing `this`
- `std::stacktrace` (planned for error reporting)
- `if consteval`
- `std::ranges` improvements

## Quick Start

### Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Run the Demo

```bash
./build/demos/http_server_demo
```

Then open http://localhost:8080 in your browser.

### Initialize (First Time)

```bash
# Create config and admin user
./build/tools/omninet-init --yes

# Generate TLS certificates
./build/tools/omninet-certs generate

# View logs
./build/tools/omninet-log tail
```

## Usage Example

```cpp
#include "OmniNet.h"
#include "http/StaticHandler.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    // Configure logging to ~/.omninet/logs/
    auto& config = Config::instance();
    auto logPath = (config.dataDir() / "logs" / "omninet.log").string();
    std::filesystem::create_directories(config.dataDir() / "logs");
    Logger::instance().setLogFile(logPath);
    Logger::instance().setLevel(LogLevel::INFO);
    
    Server::Config serverConfig;
    serverConfig.port = 8080;
    Server server(serverConfig);
    
    // Register routes
    server.get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    // Mount admin handler (provides /omninet/* endpoints)
    server.adminHandler().mount(server);
    
    // Static files
    StaticHandler staticHandler("./static");
    staticHandler.setDefaultFile("index.html");
    server.get("/*", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    server.start();
    OmniNet::cleanup();
    return 0;
}
```

## Project Structure

```
OmniNet/
├── include/           # Public headers
│   └── OmniNet/       # Public API headers
│       └── Test.h     # Test framework for users
├── src/               # Source code
│   ├── OmniNet.h      # Main header (all includes)
│   ├── config/        # Configuration system
│   ├── utils/         # Utilities (Logger, LogReader, Utils, Exception)
│   ├── crypto/        # Cryptography (Crypto, Password)
│   ├── net/           # Networking (Socket, TLSSocket, Buffer)
│   ├── http/          # HTTP server components
│   ├── websocket/     # WebSocket implementation
│   ├── db/            # Database (SQLite wrapper)
│   ├── token/         # Token management (Session tokens, API keys)
│   ├── dashboard/     # Dashboard handler
│   ├── admin/         # Admin endpoints
│   ├── sse/           # Server-Sent Events
│   └── intrusion/     # Security monitoring
├── demos/             # Example applications
├── tests/             # Unit tests
├── tools/             # CLI admin tools
├── static/            # Static files for demo
├── docs/              # Documentation
└── build/             # Build output (created by CMake)
```

## API Reference

### Server Configuration

```cpp
Server::Config config;
config.address = "127.0.0.1";
config.port = 8080;
config.workerCount = 4;
config.requestTimeout = std::chrono::seconds(30);
config.corsEnabled = true;

Server server(config);
server.start();
```

### Routing

```cpp
// GET request
server.get("/path", handler);

// POST request
server.post("/path", handler);

// Path parameters (req.pathParam(0) or req.pathParam("id"))
server.get("/users/:id", handler);

// Wildcards
server.get("/files/*", handler);
```

### Static Files

```cpp
StaticHandler staticHandler("./static");
staticHandler.setDefaultFile("index.html");

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

### WebSocket

```cpp
using namespace OmniNet::WebSocket;

WebSocket::Server wsServer;

wsServer.on("/echo", [](Connection& conn) {
    while (conn.isOpen()) {
        auto msg = conn.receiveText();
        if (msg) {
            conn.sendText(*msg);  // Echo
        }
    }
});

wsServer.start();
```

### Logging

```cpp
Logger::instance().debug("Debug message");
Logger::instance().info("Info message");
Logger::instance().errorF("Error: %s", err.c_str());

// Use config
LoggingConfig logConfig;
logConfig.level = "DEBUG";
logConfig.file = "~/.omninet/logs/omninet.log";
logConfig.maxSizeMb = 10;
logConfig.maxFiles = 5;
Logger::instance().initFromConfig(logConfig);
```

### Database

```cpp
using namespace OmniNet::DB;

Database db;
db.open("app.db");

db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
db.execute("INSERT INTO users (name) VALUES ('Alice')");

// Callback-based query
db.query("SELECT * FROM users", [](const std::vector<std::string>& row) {
    std::cout << row[0] << ": " << row[1] << std::endl;
    return true;
});

// Range-based for loop (new iterator support)
for (const auto& row : db.query("SELECT * FROM users")) {
    std::cout << row[0] << ": " << row[1] << std::endl;
}

// Prepared statements
auto stmt = db.prepare("SELECT * FROM users WHERE id = ?");
stmt.bind(1, 42);
while (stmt.step()) {
    std::string name = stmt.columnText(1);
}
```

### Session Tokens & API Keys

```cpp
using namespace OmniNet::Token;

// --- Session Tokens ---
SessionTokenStore tokenStore(db);
tokenStore.initialize();

// Create session token
std::string rawToken = tokenStore.create(
    "user_123",                     // User ID
    std::chrono::hours(24),         // TTL
    "192.168.1.1",                  // IP address
    "Mozilla/5.0",                   // User-Agent
    R"({"purpose": "web"})"         // JSON metadata
);
// IMPORTANT: rawToken is only shown ONCE at creation!

// Validate token
auto session = tokenStore.validate(rawToken);
if (session && session->isValid()) {
    std::cout << "User: " << session->userId << "\n";
    std::cout << "Expires: " << formatTime(session->expiresAt) << "\n";
}

// Revoke token
tokenStore.revoke(rawToken);

// --- API Keys ---
ApiKeyStore keyStore(db);
keyStore.initialize();

// Create API key
auto result = keyStore.create(
    "user_123",
    "Production API Key",
    {"read", "write:orders"},     // Scopes
    std::chrono::days(365)        // Expires in 1 year
);
// IMPORTANT: result.rawKey is only shown ONCE!

std::cout << "API Key: " << result.rawKey << "\n";

// Validate API key
auto validation = keyStore.validate(result.rawKey);
if (validation.valid) {
    // Check scopes
    if (keyStore.hasScope(*validation.key, "read")) {
        // Allow read operation
    }
}
```

## CLI Tools

OmniNet includes admin tools in `build/tools/`:

| Tool | Description |
|------|-------------|
| `omninet-init` | Initialize configuration and admin user |
| `omninet-status` | Show system status |
| `omninet-health` | Health check |
| `omninet-users` | User management |
| `omninet-certs` | TLS certificate management |
| `omninet-log` | View and search logs |
| `omninet-db` | Database operations |
| `omninet-ctl` | Server control (start/stop) |
| `omninet-mon` | Real-time monitoring |

### Examples

```bash
# Initialize
./build/tools/omninet-init --yes --admin-password secret

# Generate certificates
./build/tools/omninet-certs generate

# View logs
./build/tools/omninet-log tail --lines 50
./build/tools/omninet-log --level ERROR

# List users
./build/tools/omninet-users list
```

## Admin Dashboard

Access at: **http://localhost:8080/omninet**

- **Login** with admin credentials
- **Overview** - System stats and quick actions
- **Logs** - View recent logs with filtering
- **Users** - User management

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/omninet` | GET | Dashboard HTML |
| `/omninet/api/login` | POST | Login |
| `/omninet/api/logout` | POST | Logout |
| `/omninet/api/users` | GET | List users |
| `/omninet/api/users` | POST | Create user |
| `/omninet/api/users/:id` | DELETE | Delete user |
| `/omninet/api/stats` | GET | System stats |
| `/omninet/api/logs` | GET | Recent logs |

## Testing

OmniNet includes a comprehensive test suite and a public test framework for users.

### Build and Run Tests

```bash
# Build with tests
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)

# Run all tests
./build/tests/omnet_test

# Run config tests
./build/tests/test_config
```

### Using the Public Test Framework

Users can include OmniNet's test header for their own test code:

```cpp
#include <OmniNet/Test.h>

// Define a test
OMNINET_TEST(my_first_test) {
    OMNINET_ASSERT_TRUE(1 + 1 == 2);
    OMNINET_ASSERT_EQ("hello", "hello");
}

// Define another test
OMNINET_TEST(string_operations) {
    std::string s = "test";
    OMNINET_ASSERT_EQ(s.size(), 4u);
    OMNINET_ASSERT_STR_CONTAINS(s, "est");
}

// Run all registered tests
int main() {
    return OMNINET_RUN_TESTS();
}
```

### Test Macros

| Macro | Description |
|-------|-------------|
| `OMNINET_TEST(name)` | Define a test function |
| `OMNINET_REGISTER_TEST(name, func)` | Register a custom test function |
| `OMNINET_RUN_TESTS()` | Run all registered tests |

### Assertion Macros

| Macro | Description |
|-------|-------------|
| `OMNINET_ASSERT_TRUE(cond)` | Assert condition is true |
| `OMNINET_ASSERT_FALSE(cond)` | Assert condition is false |
| `OMNINET_ASSERT_EQ(a, b)` | Assert two values are equal |
| `OMNINET_ASSERT_NE(a, b)` | Assert two values are not equal |
| `OMNINET_ASSERT_GT(a, b)` | Assert a > b |
| `OMNINET_ASSERT_GE(a, b)` | Assert a >= b |
| `OMNINET_ASSERT_LT(a, b)` | Assert a < b |
| `OMNINET_ASSERT_LE(a, b)` | Assert a <= b |
| `OMNINET_ASSERT_STR_CONTAINS(str, substr)` | Assert string contains substring |
| `OMNINET_ASSERT_THROWS(expr, type)` | Assert expression throws |
| `OMNINET_ASSERT_NO_THROW(expr)` | Assert expression does not throw |

## Data Directory

OmniNet stores data in `~/.omninet/`:

```
~/.omninet/
├── config.json        # Configuration
├── logs/              # Log files
│   ├── omninet.log    # Main log
│   └── omninet.log.*  # Rotated logs
├── dashboard.db       # SQLite (users, sessions)
└── certs/             # TLS certificates
```

## Documentation

- [Demos Guide](docs/demos.md) - All demo applications
- [HTTP Server API](docs/api/http-server.md) - HTTP server reference
- [WebSocket API](docs/api/websocket.md) - WebSocket reference
- [Database API](docs/api/database.md) - SQLite wrapper
- [Logger API](docs/api/logger.md) - Logging system
- [Dashboard API](docs/api/dashboard.md) - Admin dashboard
- [Token System API](docs/api/token.md) - Session tokens and API keys
- [ThreadPool API](docs/api/threadpool.md) - Work-stealing thread pool (C++23)
- [Async Tasks API](docs/api/async.md) - Coroutine tasks and executors
- [Async I/O API](docs/api/io.md) - Non-blocking I/O with epoll
- [AsyncServer API](docs/api/async-server.md) - Event-driven HTTP server
- [Async Database API](docs/api/async-database.md) - Connection pooling

## License

MIT License