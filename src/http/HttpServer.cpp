// OmniNet - HTTP Server Implementation
#include "HttpServer.h"

#include <chrono>
#include <iostream>

namespace OmniNet::HTTP {

// ============================================================================
// Server
// ============================================================================

Server::Server(Config config)
    : m_config(std::move(config)) {
    
    m_serverSocket = std::make_unique<ServerSocket>();
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (m_running) return;
    
    // Create server socket
    m_serverSocket->setReuseAddr(m_config.reuseAddress);
    m_serverSocket->bindAndListen(m_config.port, m_config.address);
    
    m_running = true;
    m_stopping = false;
    
    // Start worker threads
    size_t workers = m_config.workerCount;
    if (workers == 0) {
        workers = std::thread::hardware_concurrency();
        if (workers == 0) workers = 4;
    }
    
    for (size_t i = 0; i < workers; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
    
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
    
    // Close server socket
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
    while (m_running) {
        auto client = m_serverSocket->accept();
        if (client) {
            ++m_stats.connectionsTotal;
            handleConnection(std::move(client));
        }
    }
}

void Server::handleConnection(std::unique_ptr<Socket> socket) {
    Connection conn(std::move(socket));
    conn.setHandler([this](const Request& req, Response& resp) {
        // Add CORS headers if enabled
        if (m_corsEnabled) {
            resp.setAccessControlAllowOrigin(m_corsAllowedOrigins);
            resp.setAccessControlAllowMethods(m_corsAllowedMethods);
            resp.setAccessControlAllowHeaders(m_corsAllowedHeaders);
        }
        
        // Let router handle the request
        m_router.handle(req, resp);
    });
    
    conn.setErrorHandler([this](const Request& req, Response& resp, std::exception_ptr) {
        if (m_errorHandler) {
            m_errorHandler(req, resp);
        } else {
            resp = Response::internalError();
        }
    });
    
    conn.setRequestTimeout(m_config.requestTimeout);
    conn.setKeepAliveTimeout(m_config.keepAliveTimeout);
    conn.setMaxRequestSize(m_config.maxRequestSize);
    
    ++m_stats.requestsActive;
    conn.processRequests();
    --m_stats.requestsActive;
    ++m_stats.requestsTotal;
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