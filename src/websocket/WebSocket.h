// OmniNet - WebSocket Connection
#ifndef OMNINET_WEBSOCKET_H
#define OMNINET_WEBSOCKET_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "net/Socket.h"
#include "net/TLSSocket.h"
#include "websocket/WebSocketFrame.h"
#include "websocket/WebSocketHandshake.h"

namespace OmniNet::WebSocket {

// ============================================================================
// WebSocket Connection
// ============================================================================

class Connection {
public:
    // --- Construction ---
    
    // Server-side: wrap accepted socket after HTTP upgrade
    Connection(std::unique_ptr<Socket> socket, std::string_view resource);
    
    // Client-side: connect and perform handshake
    Connection(
        const std::string& url,              // ws://host:port/path
        std::string_view protocol = {}       // Sec-WebSocket-Protocol
    );
    
    // Destructor
    ~Connection();
    
    // Non-copyable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // --- Lifecycle ---
    
    // Send text message
    void sendText(const std::string& message);
    
    // Send binary message
    void sendBinary(std::span<const uint8_t> data);
    
    // Send ping (with optional payload)
    void sendPing(std::span<const uint8_t> payload = {});
    
    // Send pong
    void sendPong(std::span<const uint8_t> payload = {});
    
    // Send close frame
    void close(CloseCode code = CloseCode::NORMAL, std::string_view reason = {});
    
    // Receive message
    // Returns nullopt if connection closed
    std::optional<Frame> receive();
    
    // Receive text message (blocks until text frame)
    std::optional<std::string> receiveText();
    
    // Receive binary message (blocks until binary frame)
    std::optional<std::vector<uint8_t>> receiveBinary();
    
    // --- State ---
    
    bool isOpen() const;
    std::string_view resource() const;
    std::string remoteAddress() const;
    
    // --- Ping/Pong Configuration ---
    
    // Enable automatic ping/pong
    void setAutoPing(bool enabled);
    void setPingInterval(std::chrono::seconds interval);
    void setPongTimeout(std::chrono::seconds timeout);
    
    // --- Fragmentation ---
    
    // Send large message in fragments
    void sendFragmented(
        Opcode opcode,           // TEXT or BINARY
        std::span<const uint8_t> data,
        size_t chunkSize = 4096
    );
    
    // --- Callbacks ---
    
    using TextHandler = std::function<void(std::string)>;
    using BinaryHandler = std::function<void(std::span<const uint8_t>)>;
    using CloseHandler = std::function<void(CloseCode, std::string)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using PingHandler = std::function<void(std::span<const uint8_t>)>;
    
    void onText(TextHandler handler);
    void onBinary(BinaryHandler handler);
    void onClose(CloseHandler handler);
    void onError(ErrorHandler handler);
    void onPing(PingHandler handler);
    
    // --- Statistics ---
    
    struct Stats {
        std::atomic<uint64_t> messagesSent{0};
        std::atomic<uint64_t> messagesReceived{0};
        std::atomic<uint64_t> bytesSent{0};
        std::atomic<uint64_t> bytesReceived{0};
        std::atomic<uint64_t> pingsSent{0};
        std::atomic<uint64_t> pongsReceived{0};
    };
    
    const Stats& stats() const;

private:
    // Server handshake
    void performServerHandshake();
    
    // Client handshake
    void performClientHandshake(const std::string& url);
    
    // Process incoming frame
    void processFrame(const Frame& frame);
    
    // Handle control frame
    void handleControlFrame(const Frame& frame);
    
    // Send raw frame
    void sendFrame(const Frame& frame);
    
    // Auto-ping worker
    void pingWorker();
    
    std::unique_ptr<Socket> m_socket;
    std::string m_resource;
    std::atomic<bool> m_open{false};
    bool m_isServer = true;
    
    // Handlers
    TextHandler m_textHandler;
    BinaryHandler m_binaryHandler;
    CloseHandler m_closeHandler;
    ErrorHandler m_errorHandler;
    PingHandler m_pingHandler;
    
    // Fragmentation state
    Opcode m_fragmentOpcode = Opcode::TEXT;
    bool m_fragmentStarted = false;
    std::vector<uint8_t> m_fragmentPayload;
    
    // Auto-ping
    bool m_autoPing = false;
    std::chrono::seconds m_pingInterval{30};
    std::chrono::seconds m_pongTimeout{10};
    std::thread m_pingThread;
    std::atomic<bool> m_pingRunning{false};
    std::mutex m_pingMutex;
    std::condition_variable m_pingCv;
    
    // Statistics
    Stats m_stats;
};

// ============================================================================
// WebSocket Server
// ============================================================================

class Server {
public:
    Server();
    explicit Server(const OmniNet::HTTP::Server::Config& config);
    
    // Register WebSocket endpoint
    // Handler is called for each new connection
    void on(
        std::string path,
        std::function<void(Connection& ws)> handler
    );
    
    // Start/stop
    void start();
    void stop();
    bool isRunning() const;
    
private:
    struct Endpoint {
        std::string path;
        std::function<void(Connection&)> handler;
    };
    
    std::vector<Endpoint> m_endpoints;
    std::unique_ptr<OmniNet::HTTP::Server> m_httpServer;
    std::unique_ptr<OmniNet::HTTP::Router> m_router;
};

} // namespace OmniNet::WebSocket

#endif // OMNINET_WEBSOCKET_H
