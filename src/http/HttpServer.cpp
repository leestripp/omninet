// OmniNet - HTTP Server Implementation
#include "HttpServer.h"

#include "net/TLSSocket.h"

#include <chrono>
#include <iostream>

// Debug logging
#define DEBUG_SERVER 1
#if DEBUG_SERVER
#define SERVER_DEBUG(msg) do { \
    std::cerr << "[HttpServer] " << msg << std::endl; \
} while(0)
#else
#define SERVER_DEBUG(msg) do {} while(0)
#endif

namespace OmniNet::HTTP {

// ============================================================================
// Server
// ============================================================================

Server::Server(Config config)
    : m_config(std::move(config)) {
    
    m_serverSocket = std::make_unique<ServerSocket>();
    m_adminHandler = std::make_unique<Admin::Handler>();
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (m_running) return;
    
    SERVER_DEBUG("Starting server on " << m_config.address << ":" << m_config.port);
    
    // Create server socket
    m_serverSocket->setReuseAddr(m_config.reuseAddress);
    m_serverSocket->bindAndListen(m_config.port, m_config.address);
    
    SERVER_DEBUG("Socket bound and listening");
    
    m_running = true;
    m_stopping = false;
    
    // Start worker threads
    size_t workers = m_config.workerCount;
    if (workers == 0) {
        workers = std::thread::hardware_concurrency();
        if (workers == 0) workers = 4;
    }
    
    SERVER_DEBUG("Starting " << workers << " worker threads");
    
    for (size_t i = 0; i < workers; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
    
    SERVER_DEBUG("Worker threads started, entering accept loop");
    
    // Note: Admin endpoints should be mounted by the application BEFORE start()
    // The demo calls server.adminHandler().mount(server) to register /omninet/* routes
    
    // Accept loop
    runAcceptLoop();
}

void Server::startAsync() {
    std::thread([this] { start(); }).detach();
}

void Server::stop() {
    if (!m_running) return;

    m_stopping = true;
    m_running = false;

    // Shutdown the listening socket before closing to interrupt any
    // blocking accept() call (v0.6.0 fix: without shutdown, close()
    // alone can't interrupt accept() on Linux)
    m_serverSocket->shutdown(SHUT_RDWR);
    m_serverSocket->close();

    // Join workers
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

bool Server::isRunning() const {
    return m_running.load();
}

void Server::wait() {
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

Router& Server::router() {
    return m_router;
}

void Server::get(std::string pattern, Handler handler) {
    m_router.get(std::move(pattern), std::move(handler));
}

void Server::post(std::string pattern, Handler handler) {
    m_router.post(std::move(pattern), std::move(handler));
}

void Server::put(std::string pattern, Handler handler) {
    m_router.put(std::move(pattern), std::move(handler));
}

void Server::delete_(std::string pattern, Handler handler) {
    m_router.delete_(std::move(pattern), std::move(handler));
}

void Server::patch(std::string pattern, Handler handler) {
    m_router.patch(std::move(pattern), std::move(handler));
}

void Server::head(std::string pattern, Handler handler) {
    m_router.head(std::move(pattern), std::move(handler));
}

void Server::options(std::string pattern, Handler handler) {
    m_router.options(std::move(pattern), std::move(handler));
}

void Server::any(std::string pattern, Handler handler) {
    m_router.any(std::move(pattern), std::move(handler));
}

void Server::serveStatic(std::string urlPrefix, std::string directory) {
    m_router.serveStatic(std::move(urlPrefix), std::move(directory));
}

void Server::use(Router::Middleware middleware) {
    m_router.use(std::move(middleware));
}

void Server::setNotFoundHandler(Handler handler) {
    m_router.setNotFoundHandler(std::move(handler));
    m_notFoundHandler = std::move(handler);
}

void Server::setErrorHandler(Handler handler) {
    m_router.setErrorHandler(std::move(handler));
    m_errorHandler = std::move(handler);
}

void Server::enableCORS(
    std::string_view allowedOrigins,
    std::string_view allowedMethods,
    std::string_view allowedHeaders,
    std::chrono::seconds maxAge
) {
    m_corsEnabled = true;
    m_corsAllowedOrigins = std::string(allowedOrigins);
    m_corsAllowedMethods = std::string(allowedMethods);
    m_corsAllowedHeaders = std::string(allowedHeaders);
    m_corsMaxAge = maxAge;
}

void Server::disableCORS() {
    m_corsEnabled = false;
}

const Server::Stats& Server::stats() const {
    return m_stats;
}

Admin::Handler& Server::adminHandler() {
    return *m_adminHandler;
}

const Server::Config& Server::config() const {
    return m_config;
}

uint16_t Server::port() const {
    return m_config.port;
}

std::string Server::address() const {
    return m_config.address;
}

void Server::runAcceptLoop() {
    SERVER_DEBUG("Accept loop started");
    while (m_running) {
        SERVER_DEBUG("Waiting for connection...");
        try {
            auto client = m_serverSocket->accept();
            if (client) {
                SERVER_DEBUG("Connection accepted! Spawning handler thread");
                ++m_stats.connectionsTotal;
                handleConnection(std::move(client));
            } else {
                SERVER_DEBUG("accept() returned null (no connection or timeout)");
            }
        } catch (const SocketException& e) {
            // accept() threw (e.g., socket was shut down during stop()).
            // Check if we're stopping; if so, exit the loop gracefully.
            if (m_stopping || !m_running) {
                SERVER_DEBUG("Accept loop shutting down: " << e.what());
                break;
            }
            SERVER_DEBUG("accept() error (will retry): " << e.what());
        }
    }
    SERVER_DEBUG("Accept loop ended");
}

void Server::handleConnection(std::unique_ptr<Socket> socket) {
    SERVER_DEBUG("handleConnection called, socket fd=" << socket->fd());
    
    // If TLS is enabled, wrap the socket in a TLSSocket and perform handshake
    if (m_config.tlsEnabled) {
        SERVER_DEBUG("TLS enabled, wrapping socket with TLSSocket");
        auto tlsSocket = std::make_unique<TLSSocket>(std::move(socket));
        tlsSocket->setCertFile(m_config.tlsCertFile);
        tlsSocket->setKeyFile(m_config.tlsKeyFile);
        
        // Perform TLS handshake synchronously before spawning thread
        SERVER_DEBUG("Performing TLS handshake...");
        tlsSocket->performHandshake();
        SERVER_DEBUG("TLS handshake complete, cipher: " << tlsSocket->getCipher());
        
        socket = std::move(tlsSocket);
    }
    
    // Spawn a new thread to handle this connection
    // This allows the accept loop to immediately continue accepting new connections
    std::thread([this, socket = std::move(socket)]() mutable {
        try {
            SERVER_DEBUG("Connection thread started, socket fd=" << socket->fd());

            Connection conn(std::move(socket));

            conn.setHandler([this](const Request& req, Response& resp) {
                SERVER_DEBUG("Handler called - Method: " << req.methodString() << ", Path: " << req.path());

                // Add CORS headers if enabled
                if (m_corsEnabled) {
                    resp.setAccessControlAllowOrigin(m_corsAllowedOrigins);
                    resp.setAccessControlAllowMethods(m_corsAllowedMethods);
                    resp.setAccessControlAllowHeaders(m_corsAllowedHeaders);
                }

                // Let router handle the request
                SERVER_DEBUG("Calling router.handle()");
                m_router.handle(req, resp);
                SERVER_DEBUG("Router.handle() returned, response status=" << static_cast<int>(resp.status()));
            });

            conn.setErrorHandler([this](const Request& req, Response& resp, std::exception_ptr e) {
                SERVER_DEBUG("Error handler called");
                if (m_errorHandler) {
                    m_errorHandler(req, resp);
                } else {
                    resp = Response::internalError();
                }
            });

            conn.setRequestTimeout(m_config.requestTimeout);
            conn.setKeepAliveTimeout(m_config.keepAliveTimeout);
            conn.setMaxRequestSize(m_config.maxRequestSize);

            SERVER_DEBUG("Calling conn.processRequests()");
            ++m_stats.requestsActive;
            conn.processRequests();
            --m_stats.requestsActive;
            ++m_stats.requestsTotal;
            SERVER_DEBUG("processRequests() returned, connection thread ending");
        } catch (const std::exception& e) {
            SERVER_DEBUG("Connection thread exception: " << e.what());
        } catch (...) {
            SERVER_DEBUG("Connection thread unknown exception");
        }
    }).detach();
}

void Server::workerLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Server::applySocketOptions(Socket& socket) {
    socket.setReuseAddr(m_config.reuseAddress);
    if (!m_config.useNagle) {
        socket.setTcpNoDelay(true);
    }
}

// ============================================================================
// HTTPSServer (defined in header)
// ============================================================================

// ============================================================================
// ServerBuilder
// ============================================================================

ServerBuilder& ServerBuilder::address(const std::string& addr) {
    m_config.address = addr;
    return *this;
}

ServerBuilder& ServerBuilder::port(uint16_t port) {
    m_config.port = port;
    return *this;
}

ServerBuilder& ServerBuilder::workers(size_t count) {
    m_config.workerCount = count;
    return *this;
}

ServerBuilder& ServerBuilder::requestTimeout(std::chrono::seconds timeout) {
    m_config.requestTimeout = timeout;
    return *this;
}

ServerBuilder& ServerBuilder::keepAliveTimeout(std::chrono::seconds timeout) {
    m_config.keepAliveTimeout = timeout;
    return *this;
}

ServerBuilder& ServerBuilder::tls(const std::string& certFile, const std::string& keyFile) {
    m_config.tlsEnabled = true;
    m_config.tlsCertFile = certFile;
    m_config.tlsKeyFile = keyFile;
    return *this;
}

ServerBuilder& ServerBuilder::cors(
    std::string_view origins,
    std::string_view methods,
    std::string_view headers
) {
    m_config.corsEnabled = true;
    m_config.corsAllowedOrigins = std::string(origins);
    m_config.corsAllowedMethods = std::string(methods);
    m_config.corsAllowedHeaders = std::string(headers);
    return *this;
}

std::unique_ptr<Server> ServerBuilder::build() {
    return std::make_unique<Server>(std::move(m_config));
}

} // namespace OmniNet::HTTP