# OmniNet WebSocket API

## Overview

The `OmniNet::WebSocket` namespace provides full WebSocket support with:
- Server-side WebSocket endpoints
- Client-side connections
- Automatic ping/pong keepalive
- Text and binary message support
- Message fragmentation
- Secure (WSS) connections

## Quick Start

### Server Endpoint

```cpp
#include "OmniNet.h"

using namespace OmniNet;
using namespace OmniNet::WebSocket;

int main() {
    OmniNet::init();
    
    WebSocket::Server wsServer;
    
    // Register endpoint
    wsServer.on("/echo", [](Connection& conn) {
        // Handle new connection
        while (conn.isOpen()) {
            auto msg = conn.receiveText();
            if (msg) {
                conn.sendText(*msg);  // Echo back
            }
        }
    });
    
    wsServer.start();
}
```

### Client Connection

```cpp
WebSocket::Connection conn("ws://localhost:8080/echo");

conn.onText([](std::string msg) {
    std::cout << "Received: " << msg << std::endl;
});

conn.sendText("Hello!");
```

## WebSocket Server

### Basic Setup

```cpp
WebSocket::Server server;

// Register multiple endpoints
server.on("/chat", handleChat);
server.on("/game", handleGame);
server.on("/echo", handleEcho);

server.start();
```

### Configuration

```cpp
// Using HTTP server config
HTTP::Server::Config httpConfig;
httpConfig.port = 8080;
httpConfig.tlsEnabled = true;
httpConfig.tlsCertFile = "cert.pem";
httpConfig.tlsKeyFile = "key.pem";

WebSocket::Server wsServer(httpConfig);
wsServer.on("/ws", handleConnection);
```

## WebSocket Connection

### Server-Side Connection

```cpp
server.on("/my-endpoint", [](Connection& conn) {
    std::cout << "New connection from: " << conn.remoteAddress() << std::endl;
    
    while (conn.isOpen()) {
        auto frame = conn.receive();
        if (!frame) break;
        
        // Handle frame...
        if (frame->opcode == Opcode::TEXT) {
            std::string text(frame->payload.begin(), frame->payload.end());
            std::cout << "Text: " << text << std::endl;
        }
    }
});
```

### Client-Side Connection

```cpp
// Connect to WebSocket server
Connection conn("ws://example.com/path", "chat");

// Or with custom configuration
Connection conn;
conn.connect("wss://example.com/secure-path");
conn.setAutoPing(true);
conn.setPingInterval(30s);
```

## Sending Messages

### Text Messages

```cpp
// Simple text
conn.sendText("Hello, World!");

// From string view
std::string_view message = "Hello";
conn.sendText(std::string(message));
```

### Binary Messages

```cpp
// Send binary data
std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
conn.sendBinary(data);

// From span
std::span<const uint8_t> buffer(data.data(), data.size());
conn.sendBinary(buffer);
```

### Fragmented Messages (Large Data)

```cpp
// Send large data in chunks
conn.sendFragmented(Opcode::TEXT, largeData.data(), 4096);

// For binary
conn.sendFragmented(Opcode::BINARY, largeBinaryData.data(), 8192);
```

## Receiving Messages

### Blocking Receive

```cpp
// Wait for next message
auto frame = conn.receive();
if (frame) {
    processFrame(*frame);
}

// Receive specific type
auto text = conn.receiveText();  // Blocks until TEXT frame
auto binary = conn.receiveBinary();  // Blocks until BINARY frame
```

### Non-Blocking with Callbacks

```cpp
conn.onText([](std::string msg) {
    std::cout << "Text message: " << msg << std::endl;
});

conn.onBinary([](std::span<const uint8_t> data) {
    std::cout << "Binary message: " << data.size() << " bytes" << std::endl;
});
```

### Frame-Based Receive

```cpp
while (conn.isOpen()) {
    auto optFrame = conn.receive();
    if (!optFrame) break;
    
    const auto& frame = *optFrame;
    
    switch (frame.opcode) {
        case Opcode::TEXT:
            handleText(frame.payload);
            break;
        case Opcode::BINARY:
            handleBinary(frame.payload);
            break;
        case Opcode::CLOSE:
            handleClose(frame);
            break;
        case Opcode::PING:
            // Pong is automatic if autoPing enabled
            break;
        case Opcode::PONG:
            // Response to our ping
            break;
    }
}
```

## Ping/Pong Keepalive

### Automatic Ping/Pong

```cpp
conn.setAutoPing(true);
conn.setPingInterval(30s);    // Send ping every 30 seconds
conn.setPongTimeout(10s);     // Expect pong within 10 seconds

// If pong not received, connection is considered dead
conn.onError([](const std::string& err) {
    std::cerr << "Connection lost: " << err << std::endl;
});
```

### Manual Ping

```cpp
conn.sendPing();  // No payload

// Or with payload
std::vector<uint8_t> payload = {'t', 'e', 's', 't'};
conn.sendPing(payload);
```

### Ping Callback

```cpp
conn.onPing([](std::span<const uint8_t> payload) {
    std::cout << "Ping received with payload: ";
    for (auto b : payload) std::cout << (char)b;
    std::cout << std::endl;
});
```

## Closing Connection

### Normal Close

```cpp
// Clean close with reason
conn.close(CloseCode::NORMAL, "Goodbye!");

// Shortcut
conn.close();
```

### Close Codes

```cpp
conn.close(CloseCode::NORMAL, "");           // 1000 - Normal closure
conn.close(CloseCode::GOING_AWAY, "");        // 1001 - Server shutting down
conn.close(CloseCode::PROTOCOL_ERROR, "");  // 1002 - Protocol error
conn.close(CloseCode::INVALID_DATA, "");     // 1003 - Invalid data
conn.close(CloseCode::UNKNOWN_DATA, "");      // 1007 - Unsupported data type
conn.close(CloseCode::POLICY_VIOLATION, "");  // 1008 - Policy violation
conn.close(CloseCode::TOO_LARGE, "");         // 1009 - Message too large
conn.close(CloseCode::EXTENSION_ERROR, "");  // 1010 - Extension required
conn.close(CloseCode::SERVER_ERROR, "");      // 1011 - Internal server error
conn.close(CloseCode::SERVICE_RESTART, "");   // 1012 - Service restarting
conn.close(CloseCode::TRY_AGAIN_LATER, "");  // 1013 - Temporary unavailable
conn.close(CloseCode::BAD_GATEWAY, "");        // 1014 - Invalid gateway
conn.close(CloseCode::TLS_ERROR, "");         // 1015 - TLS handshake failure
```

### Close Handler

```cpp
conn.onClose([](CloseCode code, std::string reason) {
    std::cout << "Connection closed: " << static_cast<int>(code);
    if (!reason.empty()) std::cout << " - " << reason;
    std::cout << std::endl;
});
```

## Error Handling

```cpp
conn.onError([](const std::string& err) {
    std::cerr << "WebSocket error: " << err << std::endl;
});

// Check connection state
if (!conn.isOpen()) {
    std::cerr << "Connection closed" << std::endl;
}
```

## Connection Properties

```cpp
// Check if open
if (conn.isOpen()) {
    // Connection is active
}

// Get resource path
std::string resource = std::string(conn.resource());

// Remote address
std::string remoteAddr = conn.remoteAddress();
```

## Statistics

```cpp
const auto& stats = conn.stats();
std::cout << "Messages sent: " << stats.messagesSent.load() << std::endl;
std::cout << "Messages received: " << stats.messagesReceived.load() << std::endl;
std::cout << "Bytes sent: " << stats.bytesSent.load() << std::endl;
std::cout << "Bytes received: " << stats.bytesReceived.load() << std::endl;
std::cout << "Pings sent: " << stats.pingsSent.load() << std::endl;
std::cout << "Pongs received: " << stats.pongsReceived.load() << std::endl;
```

## WebSocket Chat Example

```cpp
#include "OmniNet.h"
#include <map>

using namespace OmniNet;
using namespace OmniNet::WebSocket;

class ChatServer {
private:
    std::map<std::string, Connection*> clients;
    std::mutex mutex;
    
public:
    void start() {
        WebSocket::Server server;
        
        server.on("/chat", [this](Connection& conn) {
            onConnect(conn);
            
            while (conn.isOpen()) {
                auto msg = conn.receiveText();
                if (!msg) break;
                
                broadcast(*msg, &conn);
            }
            
            onDisconnect(conn);
        });
        
        server.start();
    }
    
private:
    void onConnect(Connection& conn) {
        std::lock_guard<std::mutex> lock(mutex);
        clients[conn.remoteAddress()] = &conn;
        
        broadcast("[Server] " + conn.remoteAddress() + " joined");
    }
    
    void onDisconnect(Connection& conn) {
        std::lock_guard<std::mutex> lock(mutex);
        clients.erase(conn.remoteAddress());
        
        broadcast("[Server] " + conn.remoteAddress() + " left");
    }
    
    void broadcast(const std::string& msg, Connection* except = nullptr) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& [addr, conn] : clients) {
            if (conn != except) {
                conn->sendText(msg);
            }
        }
    }
};

int main() {
    OmniNet::init();
    ChatServer().start();
}
```

## Secure WebSocket (WSS)

### Server with TLS

```cpp
HTTP::Server::Config config;
config.port = 8443;
config.tlsEnabled = true;
config.tlsCertFile = "/home/user/.omninet/certs/omninet.crt";
config.tlsKeyFile = "/home/user/.omninet/certs/omninet.key";

WebSocket::Server server(config);
server.on("/secure-echo", handleConnection);
server.start();
```

### Client with TLS

```cpp
// Connect to WSS endpoint
Connection conn("wss://localhost:8443/secure-echo");
conn.sendText("Hello, secure world!");
```

## Subprotocol Support

```cpp
// Server: Accept subprotocol
server.on("/game", [](Connection& conn) {
    std::cout << "Subprotocol: " << conn.subprotocol() << std::endl;
});

// Client: Request subprotocol
Connection conn("ws://localhost/endpoint", "game-protocol-v1");
```

## Frame Structure (Advanced)

```cpp
struct Frame {
    Opcode opcode;           // TEXT, BINARY, CLOSE, PING, PONG
    bool fin;               // Final frame
    bool rsv1, rsv2, rsv3;  // Extensions
    Mask mask;              // Client must mask, server must not
    uint64_t payloadLen;   // Payload length
    std::vector<uint8_t> payload;
};
```

## See Also

- [HTTP Server API](http-server.md) - HTTP server reference
- [Database API](database.md) - SQLite database access
- [Config System](../config.md) - Configuration options