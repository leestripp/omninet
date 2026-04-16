// OmniNet - HTTP Connection Handler
#ifndef OMNINET_HTTP_CONNECTION_H
#define OMNINET_HTTP_CONNECTION_H

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "net/Socket.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace OmniNet::HTTP {

// ============================================================================
// HTTP Connection Handler
// ============================================================================

class Connection {
public:
    using Handler = std::function<void(const Request&, Response&)>;
    using ErrorHandler = std::function<void(const Request&, Response&, std::exception_ptr)>;
    
    Connection(std::unique_ptr<Socket> socket);
    ~Connection();
    
    // Non-copyable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // --- Processing ---
    
    // Handle the next request (returns false if connection closed)
    bool handleNextRequest();
    
    // Process requests until connection closes
    void processRequests();
    
    // --- Configuration ---
    
    void setHandler(Handler handler);
    void setErrorHandler(ErrorHandler handler);
    
    // Timeouts
    void setRequestTimeout(std::chrono::seconds timeout);
    void setKeepAliveTimeout(std::chrono::seconds timeout);
    
    // Maximum request size
    void setMaxRequestSize(size_t maxSize);
    
    // --- Handlers ---
    
    void onRequest(Handler handler);
    void onError(ErrorHandler handler);
    
    // --- State ---
    
    bool isOpen() const;
    void close();
    
    // Get underlying socket
    const Socket& socket() const;
    
    // Statistics
    struct Stats {
        size_t requestsReceived = 0;
        size_t requestsProcessed = 0;
        size_t bytesReceived = 0;
        size_t bytesSent = 0;
        size_t errors = 0;
    };
    const Stats& stats() const;

private:
    bool readRequest();
    bool sendResponse(const Response& resp);
    void handleError(std::exception_ptr e);
    
    std::unique_ptr<Socket> m_socket;
    Handler m_handler;
    ErrorHandler m_errorHandler;
    
    std::chrono::seconds m_requestTimeout{30};
    std::chrono::seconds m_keepAliveTimeout{5};
    size_t m_maxRequestSize = 1024 * 1024;  // 1MB
    
    // Buffer for reading
    std::vector<uint8_t> m_buffer;
    size_t m_bufferPos = 0;
    
    // Current request being parsed
    Request m_currentRequest;
    
    // Pipelining support
    std::vector<Request> m_pipelinedRequests;
    
    // Statistics
    Stats m_stats;
    
    // State
    bool m_keepAlive = false;
    bool m_closeConnection = false;
};

// ============================================================================
// Connection Factory
// ============================================================================

class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    
    // Create a new connection for an accepted socket
    virtual std::unique_ptr<Connection> create(std::unique_ptr<Socket> socket) = 0;
};

// ============================================================================
// Default Connection Factory
// ============================================================================

class DefaultConnectionFactory : public ConnectionFactory {
public:
    DefaultConnectionFactory();
    
    std::unique_ptr<Connection> create(std::unique_ptr<Socket> socket) override;
    
    void setHandler(Handler handler);
    void setErrorHandler(ErrorHandler handler);
    void setRequestTimeout(std::chrono::seconds timeout);
    void setKeepAliveTimeout(std::chrono::seconds timeout);
    void setMaxRequestSize(size_t maxSize);

private:
    Handler m_handler;
    ErrorHandler m_errorHandler;
    std::chrono::seconds m_requestTimeout{30};
    std::chrono::seconds m_keepAliveTimeout{5};
    size_t m_maxRequestSize = 1024 * 1024;
};

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_CONNECTION_H
