![OmniNet Banner](docs/resources/banner.png)

# OmniNet

A modern, high-performance C++ HTTP/WebSocket server library with TLS support, session management, and more.

## Features

- **HTTP Server** - Full HTTP/1.1 server implementation with keep-alive support
- **WebSocket** - Full WebSocket protocol support with frame masking
- **TLS/HTTPS** - Secure connections using OpenSSL
- **Static File Serving** - Built-in static file handler with MIME types
- **Session Management** - SQLite-backed session store
- **User Authentication** - Password hashing with bcrypt
- **Rate Limiting** - Request rate limiting per IP
- **Dashboard** - Built-in monitoring dashboard
- **SSE (Server-Sent Events)** - Real-time server push
- **Intrusion Detection** - Basic security monitoring

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- OpenSSL 1.1.1+
- SQLite3

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run the Demo

```bash
./demos/http_server_demo
```

Then open http://localhost:8080 in your browser.

## Usage Example

```cpp
#include "OmniNet.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server::Config config;
    config.port = 8080;
    
    Server server(config);
    
    // Register routes
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    server.start();
    OmniNet::cleanup();
    return 0;
}
```

## Project Structure

```
OmniNet/
├── include/          # Public headers
├── src/              # Source code
│   ├── http/         # HTTP server components
│   ├── websocket/    # WebSocket implementation
│   ├── net/          # Networking (sockets, TLS, buffers)
│   ├── crypto/       # Cryptography utilities
│   ├── db/           # Database and storage
│   ├── dashboard/     # Dashboard handler
│   ├── sse/          # Server-Sent Events
│   ├── intrusion/     # Security monitoring
│   └── utils/         # Utilities
├── demos/            # Example applications
├── tests/            # Unit tests
├── static/           # Static files for demo
└── resources/        # Images and icons
```

## API Reference

### Server

```cpp
Server::Config config;
config.address = "127.0.0.1";
config.port = 8080;
config.workerCount = 4;
config.requestTimeout = std::chrono::seconds(30);

Server server(config);
server.start();
```

### Router

```cpp
// GET request
server.router().get("/path", handler);

// POST request
server.router().post("/path", handler);

// Path parameters
server.router().get("/users/:id", handler);  // req.param("id")

// Wildcards
server.router().get("/files/*", handler);
```

### Static Files

```cpp
StaticHandler staticHandler("./static");
staticHandler.setDefaultFile("index.html");
server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
    staticHandler.handle(req, resp);
});
```

### WebSocket

```cpp
server.router().get("/ws", [&server](const Request& req, Response& resp) {
    auto ws = WebSocket::upgrade(req, resp);
    if (ws) {
        ws->onMessage([](const std::string& msg) {
            std::cout << "Received: " << msg << std::endl;
        });
        ws->send("Hello from server!");
    }
});
```

## Testing

```bash
mkdir build && cd build
cmake -DBUILD_TESTING=ON ..
make -j$(nproc)
./tests/omnet_test
```

## License

MIT License
