// OmniNet - Server-Sent Events Handler
#ifndef OMNINET_SSE_HANDLER_H
#define OMNINET_SSE_HANDLER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "http/HttpServer.h"

namespace OmniNet::SSE {

// ============================================================================
// SSE Event
// ============================================================================

struct Event {
    std::string id;        // Event ID
    std::string event;     // Event type
    std::string data;      // Event data
    int retry = 0;         // Retry timeout (seconds)
    
    // Format as SSE wire protocol
    std::string format() const;
    
    // Static factory
    static Event comment(const std::string& text);
    static Event make(const std::string& data);
    static Event named(const std::string& type, const std::string& data);
};

// ============================================================================
// SSE Handler
// ============================================================================

class Handler {
public:
    explicit Handler(const std::string& path);
    ~Handler();
    
    // Handle HTTP request for SSE stream
    bool handleRequest(const HTTP::Request& req, HTTP::Response& resp);
    
    // --- Broadcasting ---
    
    // Broadcast event to all connected clients
    void broadcast(const Event& event);
    
    // Broadcast to specific event type
    void broadcast(const std::string& eventType, const std::string& data);
    
    // --- Client Management ---
    
    // Get number of connected clients
    size_t clientCount() const;
    
    // Remove client by ID
    void removeClient(const std::string& clientId);
    
    // --- Configuration ---
    
    // Keep-alive interval (sends comment)
    void setKeepAliveInterval(std::chrono::seconds interval);
    std::chrono::seconds keepAliveInterval() const;
    
    // Maximum event queue size per client
    void setMaxQueueSize(size_t size);
    size_t maxQueueSize() const;
    
    // --- Callbacks ---
    
    using ConnectHandler = std::function<std::string(const HTTP::Request&)>;
    using DisconnectHandler = std::function<void(const std::string& clientId)>;
    
    // Set connect handler (returns client ID)
    void onConnect(ConnectHandler handler);
    
    // Set disconnect handler
    void onDisconnect(DisconnectHandler handler);
    
    // --- Event Filtering ---
    
    // Send event only to clients subscribed to event type
    void broadcastTo(const std::string& eventType, const Event& event);
    
    // Subscribe client to event type
    void subscribe(const std::string& clientId, const std::string& eventType);
    
    // Unsubscribe client from event type
    void unsubscribe(const std::string& clientId, const std::string& eventType);

private:
    struct Client {
        std::function<bool(const std::string&)> sender;
        std::unordered_set<std::string> subscriptions;
        std::atomic<bool> connected{true};
        std::chrono::steady_clock::time_point lastActivity;
    };
    
    void addClient(const std::string& clientId, std::function<bool(const std::string&)> sender);
    void keepAliveWorker();
    
    std::string m_path;
    std::mutex m_clientsMutex;
    std::unordered_map<std::string, std::shared_ptr<Client>> m_clients;
    
    std::chrono::seconds m_keepAliveInterval{30};
    size_t m_maxQueueSize = 100;
    
    ConnectHandler m_connectHandler;
    DisconnectHandler m_disconnectHandler;
    
    std::atomic<bool> m_running{false};
    std::thread m_keepAliveThread;
};

// ============================================================================
// SSE Client (for clients connecting to SSE endpoint)
// ============================================================================

class Client {
public:
    Client(const std::string& url);
    ~Client();
    
    // Connect to SSE endpoint
    bool connect();
    
    // Disconnect
    void disconnect();
    
    // Check if connected
    bool isConnected() const;
    
    // Receive next event (blocking)
    std::optional<Event> receive();
    
    // Receive with timeout
    std::optional<Event> receive(std::chrono::seconds timeout);
    
    // --- Callbacks ---
    
    using MessageHandler = std::function<void(const Event&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using OpenHandler = std::function<void()>;
    using CloseHandler = std::function<void()>;
    
    void onMessage(MessageHandler handler);
    void onError(ErrorHandler handler);
    void onOpen(OpenHandler handler);
    void onClose(CloseHandler handler);
    
    // --- Last Event ID ---
    
    std::string lastEventId() const;
    
    // --- URL ---
    
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

// ============================================================================
// SSE Middleware
// ============================================================================

class SSEMiddleware {
public:
    SSEMiddleware(Handler& handler);
    
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);

private:
    Handler& m_handler;
};

} // namespace OmniNet::SSE

#endif // OMNINET_SSE_HANDLER_H