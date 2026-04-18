# OmniNet Demo Usage Guide

This guide covers all demo applications available in OmniNet, including how to build, run, and test them.

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.16+
- OpenSSL
- SQLite3

## Building

```bash
# Configure CMake (downloads dependencies)
cmake -S . -B build

# Build all targets
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build
```

## TLS Certificates

OmniNet includes a CLI tool to generate self-signed TLS certificates for development.

### Generate Certificates

```bash
# Generate certificates using omninet-certs tool
./build/tools/omninet-certs generate

# Or install tools first and use direct path
cmake --build build --target omninet-certs
./build/tools/omninet-certs generate
```

### Generated Files

```
~/.omninet/certs/
├── omninet.crt   # Certificate (public)
├── omninet.key   # Private key (600 permissions)
├── omninet.pem   # Combined cert+key
└── omninet.cnf   # OpenSSL config
```

### Certificate Tool Options

```bash
./build/tools/omninet-certs generate [OPTIONS]

Options:
  --dir <path>     Output directory (default: ~/.omninet/certs/)
  --name <name>    Certificate name prefix (default: omninet)
  --days <days>    Validity period (default: 365)
  --force          Overwrite existing certificates
```

### Example: Custom Certificate

```bash
# Generate for specific directory
./build/tools/omninet-certs generate --dir /path/to/certs

# Regenerate with force
./build/tools/omninet-certs generate --force
```

---

## Demo 1: HTTP Server

A basic HTTP server with API endpoints, admin routes, and static file serving.

**Location:** `demos/http_server.cpp`

### Build
```bash
cmake --build build --target http_server_demo -j$(nproc)
```

### Run
```bash
./build/demos/http_server_demo
```

### Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/hello` | JSON greeting message |
| GET | `/api/health` | Health check status |
| GET | `/api/time` | Current server timestamp |
| POST | `/api/echo` | Echoes request body as JSON |
| GET | `/` | Static index.html |
| GET | `/*` | Static file serving |
| GET | `/omninet` | Admin dashboard (HTML) |
| GET | `/omninet/api/*` | Dashboard API endpoints |
| GET | `/omninet/health` | System health (JSON) |
| GET | `/omninet/stats` | System statistics (JSON) |

### Test with curl

```bash
# Health check
curl http://localhost:8080/api/health

# Hello endpoint
curl http://localhost:8080/api/hello

# Current time
curl http://localhost:8080/api/time

# Echo POST data
curl -X POST http://localhost:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello"}'

# Admin health
curl http://localhost:8080/omninet/health

# Dashboard API
curl http://localhost:8080/omninet/api/stats
```

---

## Demo 2: HTTPS Server

An HTTPS/TLS encrypted server with secure endpoints.

**Location:** `demos/https_server.cpp`

### Prerequisites

1. Generate TLS certificates:
   ```bash
   ./build/tools/omninet-certs generate
   ```

2. Certificates are automatically loaded from `~/.omninet/certs/` by default.

### Build
```bash
cmake --build build --target https_server_demo -j$(nproc)
```

### Run
```bash
./build/demos/https_server_demo
```

### Test

```bash
# HTTPS request (ignore certificate warning for self-signed)
curl -k https://localhost:8443/

# Or with specific certificate
curl --cacert ~/.omninet/certs/omninet.crt https://localhost:8443/

# Secure API endpoint
curl -k https://localhost:8443/api/hello
```

### Available Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | HTTPS greeting page |
| GET | `/api/hello` | JSON greeting |
| GET | `/api/health` | Health check |

---

## Demo 3: Static Files Server

Serve static files from a directory.

**Location:** `demos/static_files.cpp`

### Build
```bash
cmake --build build --target static_files_demo -j$(nproc)
```

### Run

```bash
# Serve project static directory
./build/demos/static_files_demo

# Serve custom directory
./build/demos/static_files_demo /var/www/html

# Serve current directory
./build/demos/static_files_demo .
```

### Features

- MIME type detection
- Index.html fallback
- 404 for missing files
- CORS headers

### Test

```bash
# List files
curl http://localhost:8080/

# Access specific file
curl http://localhost:8080/index.html
```

---

## Demo 4: WebSocket Echo Server

WebSocket server that echoes messages back to the client.

**Location:** `demos/websocket_echo.cpp`

### Build
```bash
cmake --build build --target websocket_echo_demo -j$(nproc)
```

### Run
```bash
./build/demos/websocket_echo_demo
```

### Test with JavaScript (Browser Console)

```javascript
// Open browser console and run:
const ws = new WebSocket('ws://localhost:8080/ws');

ws.onopen = () => {
    console.log('Connected!');
    ws.send('Hello, World!');
};

ws.onmessage = (event) => {
    console.log('Received:', event.data);
};

ws.onclose = () => console.log('Disconnected');
```

### Test with wscat

```bash
# Install: npm install -g wscat
wscat -c ws://localhost:8080/ws

# Type messages and see them echoed back
> Hello
< Hello
```

---

## Demo 5: Admin Dashboard

Admin dashboard with user authentication, session management, and logs viewer.

**Location:** `demos/dashboard.cpp`

### Build
```bash
cmake --build build --target dashboard_demo -j$(nproc)
```

### Run
```bash
./build/demos/dashboard_demo
```

### Access

- **Dashboard:** http://localhost:8080/omninet
- **API:** http://localhost:8080/omninet/api

### First-Time Setup

On first run, initialize the admin user:
```bash
./build/tools/omninet-init --yes
```

Or use the dashboard UI to create a user.

### Features

- User management (CRUD)
- Session handling with TTL
- Rate limiting (per IP)
- IP-based access control
- Logs viewer tab
- System stats

### Dashboard API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/omninet` | Dashboard HTML |
| POST | `/omninet/api/login` | User login |
| POST | `/omninet/api/logout` | User logout |
| GET | `/omninet/api/users` | List users |
| POST | `/omninet/api/users` | Create user |
| DELETE | `/omninet/api/users/:id` | Delete user |
| GET | `/omninet/api/stats` | System stats |
| GET | `/omninet/api/logs` | Recent logs |

### Test

```bash
# Initialize (creates default user)
./build/tools/omninet-init --yes --admin-password mypassword

# Login via API
curl -X POST http://localhost:8080/omninet/api/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "mypassword"}'

# Access users (with X-Session-ID header from login)
curl -H "X-Session-ID: <session-id>" http://localhost:8080/omninet/api/users
```

---

## Running Multiple Demos

Each demo runs on a specific port. You can run multiple simultaneously:

| Demo | Port |
|------|------|
| HTTP Server | 8080 |
| HTTPS Server | 8443 |
| Static Files | 8080 |
| WebSocket | 8080 |
| Dashboard | 8080 |

```bash
# Run HTTP server in background
./build/demos/http_server_demo &
PID_HTTP=$!

# Run HTTPS server in background
./build/demos/https_server_demo &

# Check status
ps aux | grep demo

# Stop all
pkill -f demo
```

---

## Configuration File

OmniNet reads configuration from `~/.omninet/config.json`:

```json
{
  "server": {
    "address": "0.0.0.0",
    "port": 8080,
    "workers": 0,
    "requestTimeout": 30
  },
  "tls": {
    "enabled": true,
    "certFile": "",
    "keyFile": ""
  },
  "logging": {
    "level": "INFO"
  },
  "dashboard": {
    "enabled": true,
    "urlPath": "/omninet",
    "apiPath": "/omninet/api"
  }
}
```

### Log Files

Logs are stored at: `~/.omninet/logs/omninet.log`

- Main log: `~/.omninet/logs/omninet.log`
- Rotated logs: `~/.omninet/logs/omninet.log.1`, `.2`, etc.

View logs:
```bash
./build/tools/omninet-log tail
./build/tools/omninet-log --level ERROR
```

---

## Code Examples

### Basic HTTP Server

```cpp
#include "OmniNet.h"
#include "http/StaticHandler.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    // Load config
    auto& config = Config::instance();
    
    // Configure logging to ~/.omninet/logs/
    auto dataDir = config.dataDir();
    std::filesystem::create_directories(dataDir / "logs");
    Logger::instance().setLogFile((dataDir / "logs" / "omninet.log").string());
    Logger::instance().setLevel(LogLevel::INFO);
    
    // Create server
    Server::Config serverConfig;
    serverConfig.address = "127.0.0.1";
    serverConfig.port = 8080;
    Server server(serverConfig);
    
    // Route handlers
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    // Mount admin handler
    server.adminHandler().mount(server);
    
    // Static file serving
    StaticHandler staticHandler("./static");
    staticHandler.setDefaultFile("index.html");
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    server.start();
    OmniNet::cleanup();
    return 0;
}
```

### HTTPS Server with TLS

```cpp
#include "OmniNet.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server::Config config;
    config.port = 8443;
    config.tlsEnabled = true;
    config.tlsCertFile = Config::instance().defaultTlsCertPath().string();
    config.tlsKeyFile = Config::instance().defaultTlsKeyPath().string();
    
    Server server(config);
    
    server.router().get("/", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/html");
        resp.setBody("<!DOCTYPE html><html><body><h1>Secure Server</h1></body></html>");
    });
    
    server.start();
    OmniNet::cleanup();
    return 0;
}
```

### Route with Parameters

```cpp
server.router().get("/users/:id/posts/:postId", [](const Request& req, Response& resp) {
    std::string userId = std::string(req.pathParam(0));
    std::string postId = std::string(req.pathParam(1));
    
    resp.setStatus(StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody("{\"userId\": \"" + userId + "\", \"postId\": \"" + postId + "\"}");
});
```

### WebSocket Handler

```cpp
#include "OmniNet.h"
#include "websocket/WebSocket.h"

using namespace OmniNet::HTTP;
using namespace OmniNet::WebSocket;

int main() {
    OmniNet::init();
    
    Server::Config config;
    config.port = 8080;
    Server server(config);
    
    // WebSocket endpoint
    server.router().get("/ws", [](const Request& req, Response& resp) {
        auto ws = WebSocket::upgrade(req, resp);
        if (ws) {
            ws->onMessage([](WebSocket* ws, const std::string& msg) {
                // Echo back
                ws->send(msg);
            });
            ws->onClose([](WebSocket* ws) {
                // Handle close
            });
        }
    });
    
    server.start();
    OmniNet::cleanup();
    return 0;
}
```

### Database Integration

```cpp
#include "OmniNet.h"
#include "db/Database.h"

using namespace OmniNet::DB;

int main() {
    OmniNet::init();
    
    Database db;
    db.open("app.db");
    
    // Create table
    db.execute(R"(
        CREATE TABLE IF NOT EXISTS items (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL
        )
    )");
    
    // Insert data
    db.execute("INSERT INTO items (name) VALUES ('Test Item')");
    
    // Query data
    db.query("SELECT * FROM items", [](const std::vector<std::string>& row) {
        std::cout << row[0] << ": " << row[1] << std::endl;
        return true;
    });
    
    db.close();
    OmniNet::cleanup();
    return 0;
}
```

---

## CLI Tools

OmniNet includes several CLI tools in `build/tools/`:

| Tool | Description |
|------|-------------|
| `omninet-init` | Initialize OmniNet configuration |
| `omninet-status` | Show system status |
| `omninet-health` | Health check |
| `omninet-users` | User management |
| `omninet-certs` | TLS certificate management |
| `omninet-db` | Database management |
| `omninet-log` | Log viewer |
| `omninet-firewall` | Firewall rules |
| `omninet-ctl` | Server control |
| `omninet-mon` | Monitoring |

### Example Usage

```bash
# Initialize new installation
./build/tools/omninet-init --yes

# Check status
./build/tools/omninet-status

# View recent logs
./build/tools/omninet-log tail --lines 50

# List users
./build/tools/omninet-users list

# Generate certificates
./build/tools/omninet-certs generate
```

---

## Troubleshooting

### Port Already in Use

```bash
# Check what's using the port
lsof -i :8080

# Kill the process
kill $(lsof -t -i :8080)
```

### Certificate Errors (HTTPS)

```bash
# Regenerate certificates
./build/tools/omninet-certs generate --force

# Or check paths
cat ~/.omninet/config.json | grep -A2 tls
```

### Build Failures

```bash
# Clean and rebuild
rm -rf build
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Tests Failing

```bash
# Run specific test
./build/tests/omnet_test

# Run config tests
./build/tests/test_config
```

---

## Next Steps

- Review [API Documentation](api/index.md) for detailed usage
- Check `tests/test_all.cpp` for more examples
- Review `src/` for implementation details

---

## Quick Reference

```bash
# Build everything
cmake -S . -B build && cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Generate TLS certificates
./build/tools/omninet-certs generate

# Initialize
./build/tools/omninet-init --yes

# Run demos
./build/demos/http_server_demo
./build/demos/https_server_demo
./build/demos/websocket_echo_demo
./build/demos/dashboard_demo

# View logs
./build/tools/omninet-log tail
```