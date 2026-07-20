// OmniNet - WebSocket Connection
#ifndef OMNINET_WEBSOCKET_H
#define OMNINET_WEBSOCKET_H

#include <atomic>
#include <chrono>
#include <condition_variable>
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
#include "http/HttpServer.h"
#include "http/Router.h"

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
    virtual ~Connection();
    
    // Non-copyable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // --- Lifecycle ---

    // v0.7.0 (Phase 11b): virtual so test mocks can subclass and
    // override (the broadcast tests use a MockConnection that tracks
    // sends and can simulate failure).
    virtual void sendText(const std::string& message);

    virtual void sendBinary(std::span<const uint8_t> data);

    virtual void sendPing(std::span<const uint8_t> payload = {});

    virtual void sendPong(std::span<const uint8_t> payload = {});

    virtual void close(CloseCode code = CloseCode::NORMAL, std::string_view reason = {});
    
    // Receive message
    // Returns nullopt if connection closed
    std::optional<Frame> receive();
    
    // Receive text message (blocks until text frame)
    std::optional<std::string> receiveText();
    
    // Receive binary message (blocks until binary frame)
    std::optional<std::vector<uint8_t>> receiveBinary();
    
    // --- State ---

    // v0.7.0 (Phase 11b): virtual for testability.
    virtual bool isOpen() const;
    std::string_view resource() const;
    std::string remoteAddress() const;
    
    // --- Ping/Pong Configuration ---
    
    // Enable automatic ping/pong
    void setAutoPing(bool enabled);
    void setPingInterval(std::chrono::seconds interval);
    void setPongTimeout(std::chrono::seconds timeout);
    
    // --- Fragmentation ---
    
    // Send large message in fragments
    // Note: chunkSize is capped at 16KB to prevent memory issues
    void sendFragmented(
        Opcode opcode,           // TEXT or BINARY
        std::span<const uint8_t> data,
        size_t chunkSize = 4096
    );
    
    // Maximum chunk size for fragmented messages
    static constexpr size_t MAX_FRAGMENT_SIZE = 16384;  // 16KB
    
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

// --- Handshake (Server-side) ---
    
    // Perform server-side handshake after upgrade
    void performServerHandshake();
    
    // Client handshake
    void performClientHandshake(const std::string& url);
    
    // Process incoming frame
    void processFrame(const Frame& frame);

    // v0.7.0 (prompt.txt): blocking read loop. Reads frames from
    // the socket, decodes them via FrameCodec, and dispatches via
    // processFrame() until the connection closes or an error
    // occurs. Used by the per-path handler thread in
    // WebSocket::Server::on(). Not thread-safe — call from a
    // single thread that owns the Connection.
    void readLoop();
    
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

    // ------------------------------------------------------------------
    // v0.7.0 (Phase 11b): Client tracking + broadcast / fan-out
    // ------------------------------------------------------------------
    // The application calls addClient() from its per-connection handler
    // (after the WebSocket handshake completes) to register the
    // connection with the server. The server then tracks it and can
    // broadcast to it. removeClient() is called either explicitly or
    // automatically when a broadcast() to that client fails.

    using ClientId = uint64_t;

    // Register a connection under `path` and return its id.
    // Returns 0 on failure (e.g. null/closed connection).
    ClientId addClient(Connection& conn, std::string_view path);

    // Unregister a client. Safe to call with an unknown id (no-op).
    void removeClient(ClientId id);

    // Send a text message to a single client. No-op for unknown ids.
    void sendTo(ClientId id, std::string_view text);

    // Send a text message to all registered clients.
    // On per-client send failure, the client is auto-removed and
    // the loop continues (one dead client doesn't break the rest).
    void broadcast(std::string_view text);
    void broadcastText(std::string_view text);  // alias for clarity

    // Send a text message to all clients registered on a specific path.
    void broadcast(std::string_view text, std::string_view path);

    // Snapshot of currently-registered client ids.
    std::vector<ClientId> clients() const;
    std::vector<ClientId> clients(std::string_view path) const;

    // Number of currently-registered clients (feature #3).
    size_t connectionCount() const;
    size_t connectionCount(std::string_view path) const;

private:
    struct Endpoint {
        std::string path;
        std::function<void(Connection&)> handler;
    };

    std::vector<Endpoint> m_endpoints;
    std::unique_ptr<OmniNet::HTTP::Server> m_httpServer;
    std::unique_ptr<OmniNet::HTTP::Router> m_router;

    // Store WebSocket handlers for connection upgrade
    std::unordered_map<std::string, std::function<void(Connection&)>> m_wsHandlers;

    // v0.7.0 (Phase 11b): client tracking for broadcast.
    mutable std::mutex m_clientsMutex;
    std::unordered_map<ClientId,
        std::pair<std::string, Connection*>> m_clients;  // id -> (path, conn)
    std::atomic<ClientId> m_nextClientId{1};
};

} // namespace OmniNet::WebSocket

#endif // OMNINET_WEBSOCKET_H
