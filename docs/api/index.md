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
- Admin dashboard with log viewer
- Intrusion detection

## What's New

### v0.7.2 — 5 TLS bug fixes (recommended upgrade)

- **`TLSContext::createClient()` / `createSocket()`** — factory methods now exist (previously link failed)
- **`setALPNProtocols()`** — now actually sets ALPN (was a no-op stub causing HTTP/2 garbage on text parsers)
- **`TLSSocket::readLine()`** — override added (previously inherited base class's raw `::recv()`, returned TLS ciphertext)
- **`SSL_VERIFY_PEER` + system CA bundle** — now defaults for client contexts (previously silent cert acceptance)
- **`TLSContext`** — inherits `enable_shared_from_this` for shared SSL_CTX

See the [Migration Guide v0.7.0 → v0.7.2](../migration/v0.7.0-to-v0.7.2.md)
for the full list of fixes and recommended action items.

### v0.7.1 — 6 bug fixes

- `IntrusionDetector::checkAndBlock()` self-deadlock (HIGH) — now returns instead of hanging
- CORS allowlist parsed into `std::unordered_set` (MED) — per-request Origin lookup
- `IntrusionDetector` schema centralized via `schemaSQL()` (MED) — for migrations/introspection
- `omninet-init` dead config sections removed (MED) — `security.*`, `dashboard.*`, `intrusion.*`
- Dead `defaultPatterns()` removed (LOW)
- `omninet-firewall` build (already fixed in v0.6.0)

### v0.7.0 — WebSocket race fix

- `WebSocket::Server::on()` now uses `registerProtocolSwitch()` — atomic registration prevents TOCTOU race + single-shot erase for concurrent clients

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
    config.port = 8080;
    
    Server server(config);
    
    // Register routes
    server.get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    // Mount admin handler for /omninet/* endpoints
    server.adminHandler().mount(server);
    
    server.start();
}
```

## API Reference

| API | Description |
|-----|-------------|
| [HTTP Server](http-server.md) | HTTP/1.1 server with routing, middleware, CORS, admin endpoints |
| [Routing](routes.md) | Route registration order, patterns, and debugging |
| [WebSocket](websocket.md) | WebSocket server and client |
| [Database](database.md) | SQLite3 wrapper with transactions and migrations |
| [Token System](token.md) | Session tokens and API keys with secure storage |
| [Logger](logger.md) | Logging system with rotation |
| [Dashboard](dashboard.md) | Admin dashboard and user management |

## CLI Tools

| Tool | Description |
|------|-------------|
| `omninet-init` | Initialize OmniNet configuration |
| `omninet-status` | Show system status |
| `omninet-health` | Health check |
| `omninet-users` | User management |
| `omninet-certs` | TLS certificate management |
| `omninet-log` | Log viewer |
| `omninet-db` | Database management |
| `omninet-firewall` | Firewall rules |
| `omninet-ctl` | Server control |
| `omninet-mon` | Monitoring |

See [Demos](../demos.md) for usage examples.

## Key Classes

### HTTP Layer
- `OmniNet::HTTP::Server` - HTTP server
- `OmniNet::HTTP::Request` - HTTP request
- `OmniNet::HTTP::Response` - HTTP response
- `OmniNet::HTTP::Router` - Route handler (see [Routing](routes.md))
- `OmniNet::HTTP::StaticHandler` - Static file serving

### WebSocket Layer
- `OmniNet::WebSocket::Server` - WebSocket server
- `OmniNet::WebSocket::Connection` - WebSocket connection

### Database Layer
- `OmniNet::DB::Database` - SQLite3 database
- `OmniNet::DB::Migration` - Schema migration

### Token System Layer
- `OmniNet::Token::SessionTokenStore` - Session token storage
- `OmniNet::Token::SessionToken` - Session token structure
- `OmniNet::Token::ApiKeyStore` - API key storage
- `OmniNet::Token::ApiKey` - API key structure
- `OmniNet::Token::TokenUtils` - Token generation utilities

### Security/Dashboard Layer
- `OmniNet::Admin::Handler` - Admin endpoints (`/omninet/*`)
- `OmniNet::Dashboard::Handler` - Dashboard UI and user management
- `OmniNet::Dashboard::RateLimiter` - Rate limiting

### Utility Classes
- `OmniNet::Config` - Global configuration singleton
- `OmniNet::Logger` - Thread-safe logging with rotation
- `OmniNet::LogReader` - Log file reader
- `OmniNet::Crypto` - Cryptographic utilities
- `OmniNet::Password` - bcrypt password hashing

## Configuration

Configuration is stored in `~/.omninet/config.json`. See [Config API](config.md) for details.

## Build Dependencies

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- OpenSSL 1.1.1+
- SQLite3

## Data Directory

OmniNet stores data in `~/.omninet/`:
```
~/.omninet/
├── config.json        # Configuration
├── logs/              # Log files
│   └── omninet.log   # Main log (rotated)
├── dashboard.db      # SQLite dashboard DB
├── certs/             # TLS certificates
└── sessions/         # Session data
```