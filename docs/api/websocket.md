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
#include "websocket/WebSocket.h"

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace OmniNet::WebSocket;

int main() {
    OmniNet::init();
    
    // Create WebSocket server
    WebSocket::Server wsServer;
    
    // Register endpoint
    wsServer.on("/echo", [](Connection& conn) {
        std::cout << "New connection from: " << conn.remoteAddress() << std::endl;
        
        // Receive messages in a loop
        while (conn.isOpen()) {
            auto msg = conn.receiveText();
            if (msg) {
                std::cout << "Received: " << *msg << std::endl;
                conn.sendText(*msg);  // Echo back
            }
        }
        
        std::cout << "Connection closed" << std::endl;
    });
    
    wsServer.start();
}
```

wsServer.start();
}
```

### Internal: How Server Endpoints Work (v0.7.0+)

Behind the scenes, `Server::on(path, handler)` calls the underlying
HTTP server's `registerProtocolSwitch()` to atomically register both
the HTTP upgrade handler and the post-handshake callback in one
operation:

- **v0.7.0+ (current)**: one atomic call — no race, multi-client safe
- **v0.6.x and earlier**: two separate calls (`any()` then
  `setPostUpgradeHandler()`) — TOCTOU race window meant concurrent
  clients could see 404; the post-upgrade handler was erased after
  first use

If you were extending `WebSocket::Server` and relied on the old
two-call pattern, switch to `registerProtocolSwitch()`. The legacy
`setPostUpgradeHandler()` is still available but deprecated and
single-shot.

### Client Connection
conn.onText([](std::string msg) {
    std::cout << "Received: " << msg << std::endl;
});

conn.sendText("Hello!");
```

## WebSocket Server

### Basic Setup

```cpp
WebSocket::Server wsServer;

// Register multiple endpoints
wsServer.on("/chat", handleChat);
wsServer.on("/game", handleGame);
wsServer.on("/echo", handleEcho);

wsServer.start();
```

### With HTTP Server Configuration

```cpp
// Use HTTP server config (TLS, CORS, etc.)
HTTP::Server::Config httpConfig;
httpConfig.port = 8080;
httpConfig.tlsEnabled = true;
httpConfig.tlsCertFile = "cert.pem";
httpConfig.tlsKeyFile = "key.pem";

WebSocket::Server wsServer(httpConfig);
wsServer.on("/ws", handleConnection);
wsServer.start();
```

## WebSocket Connection

### Server-Side Connection

```cpp
wsServer.on("/endpoint", [](Connection& conn) {
    // Connection is already open at this point
    
    // Send messages
    conn.sendText("Welcome!");
    conn.sendBinary({0x01, 0x02, 0x03});
    
    // Receive in loop
    while (conn.isOpen()) {
        auto frame = conn.receive();
        if (!frame) break;
        
        if (frame->opcode == Opcode::TEXT) {
            std::string text(frame->payload.begin(), frame->payload.end());
            std::cout << "Text: " << text << std::endl;
        }
    }
});
```

### Client-Side Connection

```cpp
// Connect to server
Connection conn("ws://localhost:8080/echo");

// Register callbacks
conn.onText([](std::string msg) {
    std::cout << "Received: " << msg << std::endl;
});

conn.onBinary([](std::span<const uint8_t> data) {
    std::cout << "Binary data: " << data.size() << " bytes" << std::endl;
});

conn.onClose([](CloseCode code, std::string reason) {
    std::cout << "Closed: " << (int)code << " " << reason << std::endl;
});

// Send message
conn.sendText("Hello, Server!");

// Keep connection open and process messages
conn.receive();  // Or use callback-based approach
```

### Connection State

```cpp
if (conn.isOpen()) {
    std::cout << "Resource: " << conn.resource() << std::endl;
    std::cout << "Remote: " << conn.remoteAddress() << std::endl;
}
```

## Sending Messages

### Text Messages

```cpp
conn.sendText("Hello, World!");
conn.sendText("Line 1\nLine 2");
conn.sendText(jsonString);  // JSON
```

### Binary Messages

```cpp
std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
conn.sendBinary(data);

std::string text = "Hello";
std::span<const uint8_t> bytes(
    reinterpret_cast<const uint8_t*>(text.data()),
    text.size()
);
conn.sendBinary(bytes);
```

### Ping/Pong

```cpp
// Send ping
conn.sendPing();

// Send ping with payload
conn.sendPing({0x01, 0x02, 0x03});

// Handle incoming pings
conn.onPing([](std::span<const uint8_t> payload) {
    std::cout << "Ping received with payload" << std::endl;
});
```

### Close Connection

```cpp
conn.close(CloseCode::NORMAL, "Closing normally");
conn.close(CloseCode::GOING_AWAY, "Server shutting down");
conn.close(CloseCode::PROTOCOL_ERROR, "Invalid frame");
```

## Receiving Messages

### Blocking Receive

```cpp
while (conn.isOpen()) {
    auto msg = conn.receiveText();
    if (!msg) break;
    std::cout << "Got: " << *msg << std::endl;
}
```

### Binary Receive

```cpp
auto data = conn.receiveBinary();
if (data) {
    std::cout << "Binary: " << data->size() << " bytes" << std::endl;
}
```

### Raw Frame Processing

```cpp
while (conn.isOpen()) {
    auto frame = conn.receive();
    if (!frame) break;
    
    switch (frame->opcode) {
        case Opcode::TEXT:
            std::string text(frame->payload.begin(), frame->payload.end());
            processText(text);
            break;
        case Opcode::BINARY:
            processBinary(frame->payload);
            break;
        case Opcode::PING:
            conn.sendPong(frame->payload);
            break;
        case Opcode::PONG:
            // Auto-handled if autoPing enabled
            break;
        case Opcode::CLOSE:
            return;
    }
}
```

## Callback-Based API

### Text Handler

```cpp
conn.onText([](std::string msg) {
    std::cout << "Message: " << msg << std::endl;
    
    // Echo back
    conn.sendText("Got: " + msg);
});
```

### Binary Handler

```cpp
conn.onBinary([](std::span<const uint8_t> data) {
    std::cout << "Received " << data.size() << " bytes" << std::endl;
});
```

### Close Handler

```cpp
conn.onClose([](CloseCode code, std::string reason) {
    std::cout << "Connection closed: " << (int)code << " - " << reason << std::endl;
});
```

### Error Handler

```cpp
conn.onError([](const std::string& error) {
    std::cerr << "WebSocket error: " << error << std::endl;
});
```

## Auto Ping/Pong

Enable automatic ping/pong for connection health checking:

```cpp
conn.setAutoPing(true);
conn.setPingInterval(std::chrono::seconds(30));
conn.setPongTimeout(std::chrono::seconds(10));
```

When auto-ping is enabled, the library will:
1. Send ping frames every `pingInterval`
2. Expect pong response within `pongTimeout`
3. Close connection if pong not received

## Fragmentation

### Sending Large Messages

```cpp
// Automatically fragments large messages
std::string largeMessage = ...;
conn.sendText(largeMessage);

// Or manual fragmentation
conn.sendFragmented(Opcode::TEXT, largeData, 4096);
```

### Receiving Fragmented Messages

Fragmented messages are automatically reassembled. The `receive()` and `receiveText()` methods return complete messages:

```cpp
// This works even if message was sent in fragments
auto msg = conn.receiveText();
if (msg) {
    std::cout << "Complete message: " << *msg << std::endl;
}
```

## WebSocket Frame Opcodes

```cpp
Opcode::TEXT     // 0x1 - Text frame
Opcode::BINARY   // 0x2 - Binary frame
Opcode::CLOSE    // 0x8 - Close frame
Opcode::PING     // 0x9 - Ping frame
Opcode::PONG     // 0xA - Pong frame
```

## Close Codes

```cpp
CloseCode::NORMAL           // 1000 - Normal closure
CloseCode::GOING_AWAY       // 1001 - Server going down
CloseCode::PROTOCOL_ERROR   // 1002 - Protocol error
CloseCode::INVALID_DATA     // 1003 - Invalid data
CloseCode::NONE             // No close code
CloseCode::ABNORMAL_CLOSE   // 1006 - Abnormal closure
CloseCode::INCONSISTENT     // 1007 - Inconsistent data
CloseCode::POLICY_VIOLATION // 1008 - Policy violation
CloseCode::TOO_LARGE        // 1009 - Message too large
CloseCode::MISSING_EXT      // 1010 - Missing extension
CloseCode::INTERNAL_ERROR   // 1011 - Internal error
CloseCode::RESTART          // 1012 - Service restart
CloseCode::TRY_AGAIN        // 1013 - Try again later
CloseCode::BAD_GATEWAY      // 1014 - Bad gateway
CloseCode::TLS_ERROR        // 1015 - TLS error
```

## Statistics

Access connection statistics:

```cpp
const auto& stats = conn.stats();
std::cout << "Messages sent: " << stats.messagesSent.load() << std::endl;
std::cout << "Messages received: " << stats.messagesReceived.load() << std::endl;
std::cout << "Bytes sent: " << stats.bytesSent.load() << std::endl;
std::cout << "Bytes received: " << stats.bytesReceived.load() << std::endl;
```

## Secure WebSocket (WSS)

### Server with TLS

```cpp
HTTP::Server::Config config;
config.port = 8443;
config.tlsEnabled = true;
config.tlsCertFile = "/path/to/cert.pem";
config.tlsKeyFile = "/path/to/key.pem";

WebSocket::Server wsServer(config);
wsServer.on("/wss", handleConnection);
wsServer.start();
```

### Client with TLS

```cpp
// Connect to WSS endpoint
Connection conn("wss://localhost:8443/ws");

conn.onText([](std::string msg) {
    std::cout << msg << std::endl;
});

conn.sendText("Hello, WSS!");
```

## Full Example: Chat Server

```cpp
#include "OmniNet.h"
#include "websocket/WebSocket.h"
#include <map>

using namespace OmniNet;
using namespace OmniNet::WebSocket;

int main() {
    OmniNet::init();
    
    WebSocket::Server chatServer;
    
    // Track connections
    std::map<Connection*, std::string> nicknames;
    std::vector<Connection*> connections;
    
    chatServer.on("/chat", [](Connection& conn) {
        nicknames[&conn] = "Anonymous";
        
        // Announce join
        for (auto c : connections) {
            c->sendText("[Server] A user joined the chat");
        }
        connections.push_back(&conn);
        
        // Message loop
        while (conn.isOpen()) {
            auto msg = conn.receiveText();
            if (!msg) break;
            
            // Broadcast to all
            for (auto c : connections) {
                std::string broadcast = nicknames[&conn] + ": " + *msg;
                c->sendText(broadcast);
            }
        }
        
        // Announce leave
        for (auto c : connections) {
            c->sendText("[Server] A user left the chat");
        }
        
        // Remove from list
        connections.erase(
            std::remove(connections.begin(), connections.end(), &conn),
            connections.end()
        );
        nicknames.erase(&conn);
    });
    
    chatServer.on("/nick", [](Connection& conn) {
        auto msg = conn.receiveText();
        if (msg) {
            nicknames[&conn] = *msg;
        }
    });
    
    chatServer.start();
}
```

## Testing WebSocket

### Browser Console

```javascript
const ws = new WebSocket('ws://localhost:8080/echo');

ws.onopen = () => {
    console.log('Connected!');
    ws.send('Hello, World!');
};

ws.onmessage = (event) => {
    console.log('Received:', event.data);
};

ws.onclose = () => console.log('Disconnected');
ws.onerror = (error) => console.error('Error:', error);
```

### Using wscat

```bash
# Install: npm install -g wscat

# Connect to echo endpoint
wscat -c ws://localhost:8080/echo

# Type messages
> Hello
< Hello

# Exit
Ctrl+C
```

### Using websocat

```bash
# Install: cargo install websocat

# Connect
websocat ws://localhost:8080/echo

# Type and see echo
```