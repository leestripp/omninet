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

OmniNet includes a script to generate self-signed TLS certificates for development.

### Generate Certificates

```bash
# Generate certificates in default location (~/.omninet/certs/)
./scripts/generate-certs.sh

# Or specify custom location
./scripts/generate-certs.sh --dir /path/to/certs
```

### Generated Files

```
~/.omninet/certs/
├── omninet.crt   # Certificate (public)
├── omninet.key   # Private key (600 permissions)
├── omninet.pem   # Combined cert+key
└── omninet.cnf   # OpenSSL config
```

### Certificate Script Options

```bash
./scripts/generate-certs.sh [OPTIONS]

Options:
  -d, --dir <path>     Output directory (default: ~/.omninet/certs/)
  -n, --name <name>    Certificate name prefix (default: omninet)
  -h, --host <host>    Server hostname (default: localhost)
  -e, --email <email>  Email for certificate (default: admin@localhost)
  --days <days>        Validity period (default: 365)
  --force             Overwrite existing certificates
  -q, --quiet         Suppress output
  --help              Show this help message
```

### Example: Custom Certificate

```bash
# Generate for specific hostname
./scripts/generate-certs.sh --host myserver.local --days 730

# Generate with custom email
./scripts/generate-certs.sh --email dev@example.com
```

---

## Demo 1: HTTP Server

A basic HTTP server with API endpoints and static file serving.

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
```

---

## Demo 2: HTTPS Server

An HTTPS/TLS encrypted server.

**Location:** `demos/https_server.cpp`

### Prerequisites

1. Generate TLS certificates:
   ```bash
   # Use default location
   ./scripts/generate-certs.sh
   
   # Or create in project directory
   mkdir -p certs
   ./scripts/generate-certs.sh --dir certs
   ```

2. Update `demos/https_server.cpp` to point to your certificates:
   ```cpp
   config.tlsCertFile = "/home/user/.omninet/certs/omninet.crt";
   config.tlsKeyFile = "/home/user/.omninet/certs/omninet.key";
   ```
   Or rebuild with correct paths, or use config file.

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
curl -k https://localhost:8443/api/secure
```

### Available Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | HTTPS greeting page |
| GET | `/api/secure` | JSON response over HTTPS |

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

- Directory browsing
- MIME type detection
- Index.html fallback
- 404 for missing files

### Test

```bash
# List files (if directory listing enabled)
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
const ws = new WebSocket('ws://localhost:8080/echo');

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
wscat -c ws://localhost:8080/echo

# Type messages and see them echoed back
> Hello
< Hello
```

### Test with curl (WebSocket upgrade)

```bash
# Note: curl doesn't support WebSocket natively, use websocat
# Install: cargo install websocat
websocat ws://localhost:8080/echo
```

---

## Demo 5: Admin Dashboard

Admin dashboard with user authentication and session management.

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

- **URL:** http://localhost:8080/dashboard
- **API:** http://localhost:8080/api

### Default Credentials

On first run, you can create an admin user via the dashboard UI.

### Features

- User management (CRUD)
- Session handling
- Rate limiting
- IP-based access control

### Test

```bash
# Login
curl -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "yourpassword"}'

# Access users (with session cookie)
curl -b cookies.txt http://localhost:8080/api/users
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

# Run WebSocket server on different port
# (Edit websocket_echo.cpp to change port)

# Run HTTPS server
./build/demos/https_server_demo &

# Check status
ps aux | grep demo

# Stop all
pkill -f demo
```

---

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `OMNINET_PORT` | Default HTTP port | 8080 |
| `OMNINET_LOG_LEVEL` | Log level (DEBUG/INFO/WARN/ERROR) | INFO |
| `OMNINET_WORKERS` | Number of worker threads | 0 (auto) |
| `OMNINET_TIMEOUT` | Request timeout (seconds) | 30 |
| `OMNINET_DATA_DIR` | Data directory | ~/.omninet |
| `OMNINET_TLS_CERT` | TLS certificate path | (none) |
| `OMNINET_TLS_KEY` | TLS private key path | (none) |

### Example

```bash
# Run with custom settings
OMNINET_PORT=9090 \
OMNINET_LOG_LEVEL=DEBUG \
OMNINET_WORKERS=8 \
./build/demos/http_server_demo
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
    "certFile": "/home/user/.omninet/certs/omninet.crt",
    "keyFile": "/home/user/.omninet/certs/omninet.key"
  },
  "logging": {
    "level": "INFO",
    "file": "omninet.log"
  }
}
```

---

## Code Examples

### Basic HTTP Server

```cpp
#include "OmniNet.h"

using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server server;
    server.port(8080);
    
    // Route handlers
    server.get("/api/hello", [](const Request& req, Response& resp) {
        resp.json({{"message", "Hello, World!"}});
    });
    
    server.post("/api/echo", [](const Request& req, Response& resp) {
        auto body = req.json();
        resp.json({{"echo", body}});
    });
    
    // Start server (blocking)
    server.start();
    
    return 0;
}
```

### HTTPS Server with TLS

```cpp
#include "OmniNet.h"

using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server::Config config;
    config.port = 8443;
    config.tlsEnabled = true;
    config.tlsCertFile = Config::instance().defaultTlsCertPath().string();
    config.tlsKeyFile = Config::instance().defaultTlsKeyPath().string();
    
    Server server(config);
    
    server.get("/", [](const Request& req, Response& resp) {
        resp.setBody(R"(<!DOCTYPE html>
<html><body><h1>Secure Server</h1></body></html>)");
    });
    
    server.start();
    return 0;
}
```

### Route with Parameters

```cpp
server.get("/users/:id/posts/:postId", [](const Request& req, Response& resp) {
    std::string userId = req.param("id");
    std::string postId = req.param("postId");
    
    resp.json({
        {"userId", userId},
        {"postId", postId}
    });
});
```

### WebSocket Handler

```cpp
#include "OmniNet.h"
#include "websocket/WebSocket.h"

using namespace OmniNet::WebSocket;

int main() {
    OmniNet::init();
    
    Server wsServer;
    
    wsServer.on("/chat", [](Connection& conn) {
        std::cout << "Client connected: " << conn.remoteAddress() << std::endl;
        
        while (conn.isOpen()) {
            auto msg = conn.receiveText();
            if (msg) {
                std::cout << "Received: " << *msg << std::endl;
                conn.sendText("Echo: " + *msg);
            }
        }
    });
    
    wsServer.start();
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
    return 0;
}
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
./scripts/generate-certs.sh --force

# Or check paths in config
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

# Run with verbose output
./build/tests/test_config --verbose
```

---

## Next Steps

- Review [API Documentation](../api/index.md) for detailed usage
- Check `tests/test_all.cpp` for more examples
- Review `src/` for implementation details
- Review [Config System](../config.md) for configuration options

---

## Quick Reference

```bash
# Build everything
cmake -S . -B build && cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Generate TLS certificates
./scripts/generate-certs.sh

# Run HTTP demo
./build/demos/http_server_demo

# Run HTTPS demo
./build/demos/https_server_demo

# Run WebSocket demo
./build/demos/websocket_echo_demo

# Run Dashboard demo
./build/demos/dashboard_demo
```