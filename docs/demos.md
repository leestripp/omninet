# OmniNet Demo Usage Guide

This guide covers demo applications available in OmniNet.

## Building

```bash
# Configure and build
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
./build/tests/omnet_test

# Run demos
./build/demos/http_server_demo
```

---

## 1. HTTP Server Demo

A basic HTTP server with API endpoints.

**Run:**
```bash
./build/demos/http_server_demo
```

**Endpoints:**
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/hello` | JSON greeting |
| GET | `/api/health` | Health check |
| GET | `/api/time` | Server timestamp |
| POST | `/api/echo` | Echo request body |

---

## 2. HTTPS Server Demo

An HTTPS server (requires certificate files).

**Prerequisites:**
```bash
# Generate self-signed certificate
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
```

**Run:**
```bash
./build/demos/https_server_demo
```

---

## 3. Static File Server

Serve files from a directory.

**Run:**
```bash
# Serve current directory
./build/demos/static_files_demo

# Serve specific directory
./build/demos/static_files_demo /path/to/files
```

---

## 4. WebSocket Echo Demo

WebSocket server with echo endpoint.

**Run:**
```bash
./build/demos/websocket_echo_demo
```

**Test with JavaScript:**
```javascript
const ws = new WebSocket('ws://localhost:8080/echo');
ws.onopen = () => ws.send('Hello!');
ws.onmessage = (event) => console.log(event.data);
```

---

## 5. Admin Dashboard Demo

Admin dashboard with user authentication.

**Run:**
```bash
./build/demos/dashboard_demo
```

**Access:** `http://localhost:8080/dashboard`

---

## Code Examples

### Basic HTTP Server

```cpp
#include "OmniNet.h"
#include <thread>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server server;
    
    server.router().get("/hello", [](const Request& req, Response& resp) {
        resp.setBody("Hello, World!");
    });
    
    server.router().post("/echo", [](const Request& req, Response& resp) {
        resp.setBody(req.bodyAsString());
    });
    
    server.start();
    std::this_thread::sleep_for(std::chrono::hours(24));
    server.stop();
    
    OmniNet::cleanup();
    return 0;
}
```

### HTTP Server with Config

```cpp
Server::Config config;
config.address = "0.0.0.0";
config.port = 8080;
config.workerCount = 4;
config.corsEnabled = true;

Server server(config);
```

### Route with Parameters

```cpp
server.router().get("/users/:id", [](const Request& req, Response& resp) {
    auto id = req.pathParam(0);  // Get the :id parameter
    resp.setBody("User: " + std::string(id));
});
```

### JSON Response

```cpp
server.router().get("/api/data", [](const Request& req, Response& resp) {
    resp.setContentType("application/json");
    resp.setBody(R"({"key": "value"})");
});
```

### Static Files

```cpp
#include "http/StaticHandler.h"

StaticHandler staticHandler("./public");
staticHandler.setDefaultFile("index.html");

server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
    staticHandler.handle(req, resp);
});
```

---

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `OMNINET_LOG_LEVEL` | Log level | `info` |
| `OMNINET_PORT` | Default port | `8080` |

---

## Next Steps

- Check `src/` for the full API
- Review `tests/` for usage examples
- Generate TLS certificates for production use
