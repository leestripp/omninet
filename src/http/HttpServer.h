// OmniNet - HTTP Server
#ifndef OMNINET_HTTP_SERVER_H
#define OMNINET_HTTP_SERVER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "net/Socket.h"
#include "net/TLSSocket.h"
#include "http/Router.h"
#include "http/HttpConnection.h"

namespace OmniNet::HTTP {

// ============================================================================
// HTTP Server
// ============================================================================

class Server {
public:
    // --- Configuration ---
    
    struct Config {
        std::string address = "0.0.0.0";
        uint16_t port = 8080;
        
        // Threading
        size_t workerCount = 0;  // 0 = auto-detect
        size_t maxConnections = 1024;
        
        // Timeouts
        std::chrono::seconds requestTimeout{30};
        std::chrono::seconds keepAliveTimeout{5};
        
        // Limits
        size_t maxRequestSize = 1024 * 1024;  // 1MB
        size_t maxHeaderSize = 64 * 1024;  // 64KB
        
        // TLS
        bool tlsEnabled = false;
        std::string tlsCertFile;
        std::string tlsKeyFile;
        std::string tlsCaFile;
        bool tlsVerifyClient = false;
        
        // CORS
        bool corsEnabled = false;
        std::string corsAllowedOrigins = "*";
        std::string corsAllowedMethods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
        std::string corsAllowedHeaders = "Content-Type, Authorization";
        std::chrono::seconds corsMaxAge{86400};
        
        // Connection handling
        bool useNagle = true;  // TCP_NODELAY
        bool reuseAddress = true;
        
        // Logging
        bool logRequests = true;
        bool logErrors = true;
    };
    
    // --- Construction ---
    
    explicit Server(Config config = Config());
    ~Server();
    
    // Non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    // --- Lifecycle ---
    
    // Start the server (blocking until stopped)
    void start();
    
    // Start in background (non-blocking)
    void startAsync();
    
    // Stop the server gracefully
    void stop();
    
    // Check if server is running
    bool isRunning() const;
    
    // Block until server stops
    void wait();
    
    // --- Routing ---
    
    Router& router();
    
    // Convenience route methods
    void get(std::string pattern, Handler handler);
    void post(std::string pattern, Handler handler);
    void put(std::string pattern, Handler handler);
    void delete_(std::string pattern, Handler handler);
    void patch(std::string pattern, Handler handler);
    void head(std::string pattern, Handler handler);
    void options(std::string pattern, Handler handler);
    void any(std::string pattern, Handler handler);
    
    // Static files
    void serveStatic(std::string urlPrefix, std::string directory);
    
    // --- Middleware ---
    
    // Global middleware
    void use(Router::Middleware middleware);
    
    // --- Error Handling ---
    
    void setNotFoundHandler(Handler handler);
    void setErrorHandler(Handler handler);
    
    // --- CORS ---
    
    void enableCORS(
        std::string_view allowedOrigins = "*",
        std::string_view allowedMethods = "GET, POST, PUT, DELETE, PATCH, OPTIONS",
        std::string_view allowedHeaders = "Content-Type, Authorization",
        std::chrono::seconds maxAge = std::chrono::seconds(86400)
    );
    
    void disableCORS();
    
    // --- Statistics ---
    
    struct Stats {
        std::atomic<uint64_t> requestsTotal{0};
        std::atomic<uint64_t> requestsActive{0};
        std::atomic<uint64_t> requestsSuccess{0};
        std::atomic<uint64_t> requestsError{0};
        std::atomic<uint64_t> bytesReceived{0};
        std::atomic<uint64_t> bytesSent{0};
        std::atomic<uint64_t> connectionsTotal{0};
        std::atomic<uint64_t> connectionsActive{0};
    };
    
    const Stats& stats() const;
    
    // --- Configuration Access ---
    
    const Config& config() const;
    uint16_t port() const;
    std::string address() const;
    
private:
    void runAcceptLoop();
    void handleConnection(std::unique_ptr<Socket> socket);
    void workerLoop();
    
    void applySocketOptions(Socket& socket);
    
    Config m_config;
    Router m_router;
    
    // Server socket
    std::unique_ptr<ServerSocket> m_serverSocket;
    
    // Worker threads
    std::vector<std::thread> m_workers;
    
    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    
    std::mutex m_acceptMutex;
    std::condition_variable m_acceptCv;
    std::vector<std::unique_ptr<Socket>> m_pendingConnections;
    
    // CORS
    bool m_corsEnabled = false;
    std::string m_corsAllowedOrigins;
    std::string m_corsAllowedMethods;
    std::string m_corsAllowedHeaders;
    std::chrono::seconds m_corsMaxAge;
    
    // Error handlers
    Handler m_notFoundHandler;
    Handler m_errorHandler;
    
    // Statistics
    Stats m_stats;
    
    // Connection factory
    std::unique_ptr<ConnectionFactory> m_connectionFactory;
};

// ============================================================================
// HTTPS Server (convenience)
// ============================================================================

class HTTPSServer : public Server {
public:
    explicit HTTPSServer(Config config);
};

// ============================================================================
// Server Builder (fluent API)
// ============================================================================

class ServerBuilder {
public:
    ServerBuilder& address(const std::string& addr);
    ServerBuilder& port(uint16_t port);
    ServerBuilder& workers(size_t count);
    ServerBuilder& requestTimeout(std::chrono::seconds timeout);
    ServerBuilder& keepAliveTimeout(std::chrono::seconds timeout);
    ServerBuilder& tls(const std::string& certFile, const std::string& keyFile);
    ServerBuilder& cors(
        std::string_view origins,
        std::string_view methods,
        std::string_view headers
    );
    
    std::unique_ptr<Server> build();

private:
    Server::Config m_config;
};

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_SERVER_H