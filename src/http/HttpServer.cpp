// OmniNet - HTTP Server Implementation
#include "HttpServer.h"

#include "net/TLSSocket.h"

#include <chrono>
#include <iostream>

#ifndef _WIN32
#include <fcntl.h>   // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <unistd.h>  // pipe(), close(), write()
#endif

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

    // v0.7.0: create the self-pipe for instant accept-loop wakeup.
    // Both ends are set non-blocking so stop() can write without
    // ever blocking (even if the pipe is full from a previous run).
#ifndef _WIN32
    if (::pipe(m_wakeupPipe) == 0) {
        int flags = fcntl(m_wakeupPipe[0], F_GETFL, 0);
        if (flags >= 0) fcntl(m_wakeupPipe[0], F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(m_wakeupPipe[1], F_GETFL, 0);
        if (flags >= 0) fcntl(m_wakeupPipe[1], F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

Server::~Server() {
    stop();
#ifndef _WIN32
    if (m_wakeupPipe[0] != -1) { ::close(m_wakeupPipe[0]); m_wakeupPipe[0] = -1; }
    if (m_wakeupPipe[1] != -1) { ::close(m_wakeupPipe[1]); m_wakeupPipe[1] = -1; }
#endif
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

    // v0.7.0 (Phase 11c): fire the onReady callback now that the
    // socket is bound. The callback receives the actual bound port
    // (useful for ephemeral ports). Wrapped in try/catch so a user
    // exception doesn't kill the accept loop.
    if (m_onReady) {
        try {
            m_onReady(m_serverSocket->localPort());
        } catch (...) {
            SERVER_DEBUG("onReady callback threw (swallowed)");
        }
    }

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

    // v0.7.0: wake the accept loop INSTANTLY via the self-pipe trick.
    // Writing to m_wakeupPipe[1] makes poll() return immediately so
    // runAcceptLoop() can observe m_running=false and exit. No polling
    // delay, no reliance on shutdown() unblocking accept().
#ifndef _WIN32
    if (m_wakeupPipe[1] != -1) {
        const char byte = 'x';
        ssize_t w = ::write(m_wakeupPipe[1], &byte, 1);
        (void)w;  // EAGAIN is fine — the loop will still time out and exit
    }
#endif

    // Defense-in-depth: shutdown() the listening socket so a blocked
    // accept() returns EINVAL. We do NOT close() here — closing an
    // fd while another thread is blocked in poll() on it is
    // undefined behavior on Linux (poll may never return). The
    // ServerSocket destructor will close the fd safely after the
    // accept thread has exited.
    if (m_serverSocket && m_serverSocket->fd() != INVALID_SOCKET) {
        m_serverSocket->shutdown(SHUT_RDWR);
    }

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

// v0.7.0 (Phase 11c): register a callback for when the server is
// fully bound and ready to accept.
void Server::setOnReady(ReadyCallback cb) {
    m_onReady = std::move(cb);
}

// v0.7.0 (prompt.txt Bug #2 + #5): register a post-upgrade
// handoff for `path`. When a request to this path results in a
// 101 response, the server writes the 101 and then calls `cb`
// with the raw socket. The callback takes ownership.
void Server::setPostUpgradeHandler(std::string path, PostUpgradeCallback cb) {
    m_postUpgradeHandlers[std::move(path)] = std::move(cb);
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

// v0.7.0 (Phase 11a): actual bound port (getsockname()). 0 before
// start(). For an OS-assigned port (Config::port = 0), this is the
// only way to learn which port the kernel picked.
uint16_t Server::boundPort() const {
    if (!m_serverSocket) return 0;
    return m_serverSocket->localPort();
}

std::string Server::address() const {
    return m_config.address;
}

void Server::runAcceptLoop() {
    SERVER_DEBUG("Accept loop started");
    // v0.7.0: self-pipe trick. The accept loop polls BOTH the
    // listening socket AND m_wakeupPipe[0]. When stop() writes to
    // m_wakeupPipe[1], poll() returns immediately and we exit —
    // instant clean shutdown, no polling delay.
    int spin_count = 0;  // v0.7.0: track consecutive empty accepts
    while (m_running) {
        SERVER_DEBUG("Waiting for connection...");
        std::unique_ptr<Socket> client;
        try {
#ifndef _WIN32
            if (m_wakeupPipe[0] != -1) {
                client = m_serverSocket->acceptOrWakeup(
                    m_wakeupPipe[0], std::chrono::milliseconds(1000));
            } else {
                client = m_serverSocket->acceptWithTimeout(
                    std::chrono::milliseconds(100));
            }
#else
            client = m_serverSocket->acceptWithTimeout(
                std::chrono::milliseconds(100));
#endif
        } catch (const SocketException& e) {
            // accept() threw (e.g., socket was shut down during stop()).
            // Check if we're stopping; if so, exit the loop gracefully.
            if (m_stopping || !m_running) {
                SERVER_DEBUG("Accept loop shutting down: " << e.what());
                break;
            }
            SERVER_DEBUG("accept() error (will retry): " << e.what());
            continue;
        }
#ifndef _WIN32
        // Drain the wakeup pipe so the next stop() can write again.
        if (m_wakeupPipe[0] != -1) {
            char drain[16];
            while (::read(m_wakeupPipe[0], drain, sizeof(drain)) > 0) {}
        }
        // If stop() was called (m_running false), exit now — don't
        // accept any more connections.
        if (!m_running) break;
#endif
        if (!client) {
            // Timeout or wakeup — just loop and re-check m_running.
            spin_count = 0;
            continue;
        }

        // v0.7.0 fix (test_websocket_echo hang): before spawning a
        // handler thread, peek the socket to detect connections
        // whose peer has already closed (e.g., the test sent a
        // request and then closed before we could accept).
        // Without this, we'd spawn a handler thread that immediately
        // fails on recv(), and the accept loop would spin tightly
        // on the same dead connection, starving other threads
        // (notably the test thread, causing sleep_for() to never
        // return). MSG_PEEK | MSG_DONTWAIT is non-blocking and
        // doesn't consume the data, so a real request still works.
        {
            char peek;
            ssize_t n = ::recv(client->fd(), &peek, 1,
                                MSG_PEEK | MSG_DONTWAIT);
            if (n == 0) {
                // Peer closed before we could read anything.
                SERVER_DEBUG("Accepted connection already closed, dropping");
                client.reset();
                spin_count++;
                continue;
            }
            // n < 0 with EAGAIN/WOULDBLOCK means no data yet but
            // the connection is alive — proceed normally.
        }
        spin_count = 0;

        // v0.7.0: CPU-yield guard. If we've been spinning (e.g.,
        // poll() keeps reporting POLLIN for stale connections that
        // the MSG_PEEK above doesn't catch), sleep 10ms periodically
        // to let other threads (test thread, handler threads) run.
        // The accept loop is the main thread of the server in
        // blocking-start mode, so starving it starves everything.
        spin_count++;
        if (spin_count > 50) {
            SERVER_DEBUG("Accept loop spin detected, yielding CPU");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            spin_count = 0;
        }

        SERVER_DEBUG("Connection accepted! Spawning handler thread");
        ++m_stats.connectionsTotal;
        handleConnection(std::move(client));
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

            conn.setHandler([this, &conn](const Request& req, Response& resp) {
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

                // v0.7.0 (prompt.txt Bug #2 + #5): protocol switch.
                // If the route handler set a 101 response (via
                // Response::markProtocolSwitch) and a post-upgrade
                // callback is registered for this path, wire it up
                // on the Connection. After writing the 101, the
                // Connection will release the socket and call the
                // callback (which creates the WebSocket::Connection
                // and runs the per-path handler).
                if (resp.isProtocolSwitch()) {
                    auto it = m_postUpgradeHandlers.find(std::string(req.path()));
                    if (it != m_postUpgradeHandlers.end()) {
                        SERVER_DEBUG("Protocol switch: registering post-upgrade for " << req.path());
                        conn.setPostUpgradeCallback(std::move(it->second));
                        m_postUpgradeHandlers.erase(it);
                    } else {
                        SERVER_DEBUG("Protocol switch requested but no post-upgrade handler for " << req.path());
                    }
                }
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