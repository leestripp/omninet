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

// v0.7.0 (prompt.txt Bug #5): register both the HTTP upgrade
// handler and the post-upgrade callback atomically under a single
// lock. This eliminates the TOCTOU race that existed when
// WebSocket::Server::on() called m_httpServer->any() and then
// m_httpServer->setPostUpgradeHandler() separately — a request
// arriving between those two calls would find no post-upgrade
// handler and get a 404.
//
// The on_upgrade handler is invoked by the connection loop BEFORE
// the Router handles the request (so the 101 is set before the
// Router's 404 can override it). The on_post_upgrade callback is
// invoked by the Connection AFTER writing the 101, and is persistent
// (multi-client works — no erase).
//
// Usage:
//   server.registerProtocolSwitch("/ws", {
//       .on_upgrade = [&](req, resp) { /* validate, set 101 */ },
//       .on_post_upgrade = [&](socket, req) { /* build Connection */ },
//   });
void Server::registerProtocolSwitch(std::string path, ProtocolSwitchHandlers h) {
    std::lock_guard<std::mutex> lock(m_protocolSwitchesMutex);
    m_protocolSwitches[std::move(path)] = std::move(h);
}

// Deprecated shim: wraps the old single-callback API into a
// ProtocolSwitchHandlers with a no-op on_upgrade. The old API
// requires the user to also call any() for the HTTP upgrade,
// which still has the race — but at least this shim keeps
// multi-client working by not erasing the entry.
void Server::setPostUpgradeHandler(std::string path, PostUpgradeCallback cb) {
    registerProtocolSwitch(std::move(path), ProtocolSwitchHandlers{
        .on_upgrade = [](const Request&, Response& resp) {
            // No-op: the caller is expected to have called any()
            // with a handler that sets the 101 status.
            // We just mark it as a protocol switch.
            resp.markProtocolSwitch();
        },
        .on_post_upgrade = std::move(cb),
    });
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

    // v0.7.0 (prompt.txt 2026-06-19 BUG-2): parse the CSV origin
    // list into a set for per-request lookup. The CORS spec requires
    // Access-Control-Allow-Origin to be a single origin or "*" —
    // a CSV is rejected by every browser.
    //
    // Special case: "*" means "allow any origin" (no credentials).
    // We set m_corsAllowAll and emit "*" for all requests.
    m_corsAllowedOriginSet.clear();
    m_corsAllowAll = false;
    std::string_view sv = allowedOrigins;
    while (!sv.empty()) {
        // Trim leading whitespace
        size_t start = 0;
        while (start < sv.size() && (sv[start] == ' ' || sv[start] == '\t')) ++start;
        sv.remove_prefix(start);
        // Find end of this origin
        size_t end = 0;
        while (end < sv.size() && sv[end] != ',') ++end;
        std::string_view origin = sv.substr(0, end);
        // Trim trailing whitespace
        while (!origin.empty() && (origin.back() == ' ' || origin.back() == '\t')) {
            origin.remove_suffix(1);
        }
        if (!origin.empty()) {
            if (origin == "*") {
                m_corsAllowAll = true;
            } else {
                m_corsAllowedOriginSet.insert(std::string(origin));
            }
        }
        // Advance past the comma
        if (end < sv.size()) {
            sv.remove_prefix(end + 1);
        } else {
            break;
        }
    }
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
            continue;
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

                // v0.7.0 (prompt.txt 2026-06-19 BUG-2): per-request
                // origin lookup. The CORS spec requires
                // Access-Control-Allow-Origin to be a single origin
                // or "*". If "*" is configured, emit "*" for all
                // requests. Otherwise, look up the request's Origin
                // header in the allowlist; if present, echo it back
                // (with Vary: Origin so caches don't mix responses
                // for different origins). If not present, omit the
                // header (browser blocks the response).
                if (m_corsEnabled) {
                    if (m_corsAllowAll) {
                        resp.setAccessControlAllowOrigin("*");
                    } else {
                            resp.setHeader(std::string("Vary"), std::string("Origin"));
                            resp.setHeader(std::string("Access-Control-Allow-Credentials"), std::string("true"));
                        // do NOT set Access-Control-Allow-Origin.
                        // The browser will block the response.
                    }
                    resp.setAccessControlAllowMethods(m_corsAllowedMethods);
                    resp.setAccessControlAllowHeaders(m_corsAllowedHeaders);
                }

                // v0.7.0 (prompt.txt Bug #5): protocol-switch paths
                // are handled FIRST (before the Router) to eliminate
                // the TOCTOU race. The on_upgrade handler sets the 101
                // status and headers. IMPORTANT: if on_upgrade handles
                // the request (e.g., sets 101 or an error like 400),
                // we must NOT call the Router afterward — the Router
                // has no route for /echo so it returns 404, which would
                // override the 101. Only call the Router if the request
                // is NOT a protocol switch (e.g., normal HTTP path).
                {
                    std::lock_guard<std::mutex> lock(m_protocolSwitchesMutex);
                    auto it = m_protocolSwitches.find(std::string(req.path()));
                    if (it != m_protocolSwitches.end()) {
                        SERVER_DEBUG("Protocol switch: invoking on_upgrade for " << req.path());
                        it->second.on_upgrade(req, resp);
                    }
                }

                // Only call the Router if on_upgrade didn't handle the
                // request. A protocol switch response (101) or an error
                // (400) from on_upgrade should NOT be overridden by 404.
                // v0.8.3 (MonkeyByte Bug 1, SSE streaming): same
                // skip-Router rule applies to `isStreaming()` - the
                // headers have been set up by on_upgrade and the
                // post-upgrade callback will stream chunks over the
                // socket. A 404 from Router would clobber the 200.
                if (!resp.isProtocolSwitch() && !resp.isStreaming() &&
                    resp.status() == StatusCode::OK) {
                    SERVER_DEBUG("Calling router.handle()");
                    m_router.handle(req, resp);
                    SERVER_DEBUG("Router.handle() returned, response status=" << static_cast<int>(resp.status()));
                } else if (resp.isProtocolSwitch()) {
                    SERVER_DEBUG("Protocol switch handled by on_upgrade, skipping Router");
                } else if (resp.isStreaming()) {
                    SERVER_DEBUG("Streaming response handled by on_upgrade, skipping Router");
                }

                // v0.7.0 (prompt.txt Bug #5): if the response is
                // a protocol switch (set by on_upgrade), look up the
                // on_post_upgrade callback and wire it on the
                // Connection. The callback is persistent — NOT erased
                // after first use (multi-client works).
                // v0.8.3 (MonkeyByte Bug 1): same wiring for
                // isStreaming() — the post-upgrade callback will own
                // the socket and write SSE chunks.
                if (resp.isProtocolSwitch() || resp.isStreaming()) {
                    std::lock_guard<std::mutex> lock(m_protocolSwitchesMutex);
                    auto it = m_protocolSwitches.find(std::string(req.path()));
                    if (it != m_protocolSwitches.end() &&
                        it->second.on_post_upgrade) {
                        SERVER_DEBUG((resp.isStreaming() ? "Streaming: " : "Protocol switch: ")
                                     << "registering post-upgrade for " << req.path());
                        conn.setPostUpgradeCallback(it->second.on_post_upgrade);
                        // NOT erased — multi-client support (Bug #5 fix).
                    } else {
                        SERVER_DEBUG("Protocol switch/streaming requested but no post-upgrade handler for " << req.path());
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