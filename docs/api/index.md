# OmniNet API Documentation

## Overview

OmniNet is a C++ HTTP/WebSocket server library with built-in support for:
- HTTP/1.1 server with routing and middleware
- WebSocket server and client
- SQLite3 database
- TLS/HTTPS and WSS
- CORS support
- Session management
- Rate limiting
- Intrusion detection

## Quick Start

```cpp
#include "OmniNet.h"

using namespace OmniNet::HTTP;

int main() {
    OmniNet::init();
    
    Server server;
    server.port(8080);
    
    server.get("/api/hello", [](const Request& req, Response& resp) {
        resp.json({{"message", "Hello, World!"}});
    });
    
    server.start();
}
```

## API Reference

| API | Description |
|-----|-------------|
| [HTTP Server](http-server.md) | HTTP/1.1 server with routing, middleware, CORS |
| [WebSocket](websocket.md) | WebSocket server and client |
| [Database](database.md) | SQLite3 wrapper with transactions and migrations |

## Related Documentation

| Topic | Description |
|-------|-------------|
| [Config System](../config.md) | Configuration file and environment variables |
| [Installation](../installation.md) | Building and installing OmniNet |

## Key Classes

### HTTP Layer
- `OmniNet::HTTP::Server` - HTTP server
- `OmniNet::HTTP::Request` - HTTP request
- `OmniNet::HTTP::Response` - HTTP response
- `OmniNet::HTTP::Router` - Route handler

### WebSocket Layer
- `OmniNet::WebSocket::Server` - WebSocket server
- `OmniNet::WebSocket::Connection` - WebSocket connection

### Database Layer
- `OmniNet::DB::Database` - SQLite3 database
- `OmniNet::DB::Migration` - Schema migration

### Utility Classes
- `OmniNet::Config` - Global configuration
- `OmniNet::Logger` - Logging
- `OmniNet::Crypto` - Cryptographic utilities
- `OmniNet::Dashboard::Handler` - Admin dashboard