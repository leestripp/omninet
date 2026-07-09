// OmniNet - Server-Sent Events Handler
#ifndef OMNINET_SSE_HANDLER_H
#define OMNINET_SSE_HANDLER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "http/HttpServer.h"
#include "net/Socket.h"

namespace OmniNet::SSE {

// ============================================================================
// SSE Event
// ============================================================================

struct Event {
    std::string id;        // Event ID
    std::string event;     // Event type
    std::string data;      // Event data
    int retry = 0;         // Retry timeout (seconds)

    // Format as SSE wire protocol (per WHATWG HTML spec).
    std::string format() const;

    // Static factories.
    static Event comment(const std::string& text);  // data = ":text"
    static Event make(const std::string& data);
    static Event named(const std::string& type, const std::string& data);
};

// ============================================================================
// SSE Handler
// ============================================================================
//
// Owns a map of connected SSE clients. For each client we store a
// callback (`Sender`) that writes one formatted event to that client's
// socket. The Handler itself doesn't own sockets — sockets are owned
// by per-client worker threads started by `attach()`.
//
// Thread-safety: `addClient`, `removeClient`, `broadcast`, and
// `clientCount` are all safe to call from any thread, including
// concurrently.

class Handler {
public:
    // Sender: write the formatted event bytes to the underlying socket.
    // Should throw or return false on socket error.
    using Sender = std::function<bool(const std::string&)>;

    explicit Handler(const std::string& path);
    ~Handler();

    // v0.8.3 (MonkeyByte Bug 1): register this handler as an SSE
    // endpoint on the given HTTP server. Wires:
    //   1) on_upgrade: build 200 OK + Transfer-Encoding: chunked + mark streaming
    //   2) on_post_upgrade: spawn worker thread that owns the socket
    //      and writes SSE chunks until the client disconnects.
    // Idempotent: safe to call more than once (last call wins).
    void attach(HTTP::Server& server);

    // v0.8.3 (MonkeyByte Bug 1): per-connection worker entry point.
    // Public so it can be unit-tested with a mock socket.
    // Blocks until the client disconnects.
    void runClientStream(std::unique_ptr<Socket> socket,
                         const HTTP::Request& req);

    // --- Broadcasting ---

    // Broadcast event to all connected clients.
    void broadcast(const Event& event);
    // Convenience: broadcast a named string event.
    void broadcast(const std::string& eventType, const std::string& data);

    // --- Client Management ---

    // Register a client. Public so tests and programmatic use (non-HTTP)
    // can register senders directly. If the same clientId is already
    // registered, the old entry is replaced.
    void addClient(const std::string& clientId, Sender sender);

    // Remove a client. Called by the worker thread when its
    // connection closes (or by user code). Idempotent.
    void removeClient(const std::string& clientId);
    size_t clientCount() const;
    size_t connectionCount() const { return clientCount(); }  // alias

    // --- Callbacks ---

    // Called for each new client connection. If it returns a non-empty
    // string, that string is used as the client ID (otherwise a random
    // URL-safe 16-byte ID is generated).
    using ConnectHandler = std::function<std::string(const HTTP::Request&)>;
    using DisconnectHandler = std::function<void(const std::string& clientId)>;
    void onConnect(ConnectHandler handler);
    void onDisconnect(DisconnectHandler handler);

private:
    // Connected client state (private — internal only).
    struct Client {
        Sender sender;
        std::atomic<bool> connected{true};
        std::chrono::steady_clock::time_point lastActivity;
    };

    // Write one chunked-transfer-encoding frame to a socket.
    // hex-size + "\r\n" + data + "\r\n".
    static bool writeChunked(Socket& sock, std::string_view data);
    // Write the chunked stream terminator: "0\r\n\r\n".
    static void writeChunkedEnd(Socket& sock);

    std::string m_path;
    mutable std::mutex m_clientsMutex;
    std::unordered_map<std::string, std::shared_ptr<Client>> m_clients;

    ConnectHandler m_connectHandler;
    DisconnectHandler m_disconnectHandler;
};

// ============================================================================
// SSE Client (outbound, for connecting to a remote SSE endpoint)
// ============================================================================

class Client {
public:
    Client(const std::string& url);
    ~Client();

    bool connect();
    void disconnect();
    bool isConnected() const;

    std::optional<Event> receive();
    std::optional<Event> receive(std::chrono::seconds timeout);

    using MessageHandler = std::function<void(const Event&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using OpenHandler = std::function<void()>;
    using CloseHandler = std::function<void()>;

    void onMessage(MessageHandler handler);
    void onError(ErrorHandler handler);
    void onOpen(OpenHandler handler);
    void onClose(CloseHandler handler);

    std::string lastEventId() const;
    std::string url() const;

private:
    void runReceiveLoop();
    void parseEventStream(const std::string& data);

    std::string m_url;
    std::atomic<bool> m_connected{false};
    std::unique_ptr<Socket> m_socket;

    MessageHandler m_messageHandler;
    ErrorHandler m_errorHandler;
    OpenHandler m_openHandler;
    CloseHandler m_closeHandler;

    std::thread m_receiveThread;
    std::mutex m_eventMutex;
    std::condition_variable m_eventCv;
    std::vector<Event> m_eventQueue;

    std::string m_lastEventId;
    std::string m_currentEvent;
    std::string m_currentData;
    std::string m_currentId;
    std::string m_currentType;
};

// SSEMiddleware was removed in v0.8.3 (MonkeyByte Bug 1) — the
// HTTP::Server::registerProtocolSwitch mechanism supersedes the
// old middleware-based approach. See Handler::attach() above.

} // namespace OmniNet::SSE

#endif // OMNINET_SSE_HANDLER_H