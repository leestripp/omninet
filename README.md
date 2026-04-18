![OmniNet Banner](docs/resources/banner.png)

# OmniNet

A modern, high-performance C++ HTTP/WebSocket server library with TLS support, session management, admin dashboard, and more.

## Features

- **HTTP Server** - Full HTTP/1.1 server implementation with keep-alive support
- **WebSocket** - Full WebSocket protocol support (RFC 6455)
- **TLS/HTTPS** - Secure connections using OpenSSL
- **Static File Serving** - Built-in static file handler with MIME types
- **Session Management** - SQLite-backed session store
- **User Authentication** - Password hashing with bcrypt
- **Rate Limiting** - Request rate limiting per IP
- **Admin Dashboard** - Web-based monitoring at `/omninet`
- **Log Viewer** - View logs in dashboard with filtering
- **SSE (Server-Sent Events)** - Real-time server push
- **Intrusion Detection** - Basic security monitoring
- **Log Rotation** - Automatic rotation based on file size
- **CLI Tools** - Admin tools for users, certs, logs, etc.

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- OpenSSL 1.1.1+
- SQLite3

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
├── src/              # Source code
│   ├── OmniNet.h     # Main header (all includes)
│   ├── config/       # Configuration system
│   ├── utils/        # Utilities (Logger, LogReader, Utils, Exception)
│   ├── crypto/       # Cryptography (Crypto, Password)
│   ├── net/          # Networking (Socket, TLSSocket, Buffer)
│   ├── http/         # HTTP server components
│   ├── websocket/    # WebSocket implementation
│   ├── db/           # Database (SQLite wrapper)
│   ├── dashboard/     # Dashboard handler
│   ├── admin/        # Admin endpoints
│   ├── sse/          # Server-Sent Events
│   └── intrusion/     # Security monitoring
├── demos/            # Example applications
├── tests/            # Unit tests
├── tools/            # CLI admin tools
├── static/           # Static files for demo
├── docs/             # Documentation
└── build/            # Build output (created by CMake)
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

db.query("SELECT * FROM users", [](const std::vector<std::string>& row) {
    std::cout << row[0] << ": " << row[1] << std::endl;
    return true;
});
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

## Data Directory

OmniNet stores data in `~/.omninet/`:

```
~/.omninet/
├── config.json        # Configuration
├── logs/               # Log files
│   ├── omninet.log    # Main log
│   └── omninet.log.*  # Rotated logs
├── dashboard.db       # SQLite (users, sessions)
└── certs/             # TLS certificates
```

## Testing

```bash
# Build with tests
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test

# Run specific tests
./build/tests/test_config
```

## Documentation

- [Demos Guide](docs/demos.md) - All demo applications
- [HTTP Server API](docs/api/http-server.md) - HTTP server reference
- [WebSocket API](docs/api/websocket.md) - WebSocket reference
- [Database API](docs/api/database.md) - SQLite wrapper
- [Logger API](docs/api/logger.md) - Logging system
- [Dashboard API](docs/api/dashboard.md) - Admin dashboard

## License

MIT License