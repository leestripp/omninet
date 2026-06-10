// OmniNet - Async HTTP Server Implementation
#include "AsyncServer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>

#include "../utils/Exception.h"

namespace OmniNet::Server {

// ============================================================================
// Construction / Destruction
// ============================================================================

AsyncServer::AsyncServer(IO::IoContext& ctx) : AsyncServer(ctx, Config{}) {}

AsyncServer::AsyncServer(IO::IoContext& ctx, Config config)
    : m_config(std::move(config)), m_context(ctx) {
    // Create listening socket
    m_listenSocket = std::make_unique<Socket>();

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket: " +
                                  std::string(std::strerror(errno)));
    }

    if (m_config.reuseAddress) {
        int opt = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    if (!m_config.useNagle) {
        int opt = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.port);
    if (m_config.address == "0.0.0.0" || m_config.address.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        ::inet_pton(AF_INET, m_config.address.c_str(), &addr.sin_addr);
    }

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error("Failed to bind to " + m_config.address + ":" +
                                  std::to_string(m_config.port) + ": " +
                                  std::strerror(errno));
    }

    if (::listen(fd, 128) < 0) {
        ::close(fd);
        throw std::runtime_error("Failed to listen: " +
                                  std::string(std::strerror(errno)));
    }

    // Set non-blocking
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    // Hand the fd to our Socket wrapper
    m_listenSocket = std::make_unique<Socket>(fd);
}

AsyncServer::~AsyncServer() {
    stop();
    if (m_listenSocket) {
        m_listenSocket->close();
    }
}

// ============================================================================
// Routing
// ============================================================================

void AsyncServer::get(std::string_view pattern, Async::AsyncHandler handler) {
    m_router.get(std::string(pattern), [handler = std::move(handler)]
                                        (const HTTP::Request& req, HTTP::Response& resp) {
        // This won't be called - we use async routing directly
        (void)req;
        (void)resp;
        (void)handler;
    });
    // Store async handler in separate map
    // For simplicity, we use a different mechanism
    // TODO: Add async route storage
}

// ... similar for other methods ...

// ============================================================================
// Lifecycle
// ============================================================================

void AsyncServer::run() {
    if (m_running.exchange(true)) {
        return;  // Already running
    }

    // Start accept loop
    auto acceptTask = accept_loop();
    // Run the event loop in this thread
    m_context.run(m_stopSource.get_token());
    m_running.store(false);
}

void AsyncServer::run_async() {
    m_eventLoopThread = std::thread([this]() {
        run();
    });
}

void AsyncServer::stop() {
    m_stopping.store(true);
    m_stopSource.request_stop();
    m_context.stop();

    if (m_eventLoopThread.joinable()) {
        m_eventLoopThread.join();
    }
}

void AsyncServer::wait() {
    if (m_eventLoopThread.joinable()) {
        m_eventLoopThread.join();
    }
}

// ============================================================================
// Accept loop
// ============================================================================

Async::Task<> AsyncServer::accept_loop() {
    // For now, just accept in a loop
    // TODO: Post the accept task back to the event loop
    co_return;
}

// ============================================================================
// Connection handling
// ============================================================================

Async::Task<> AsyncServer::handle_connection(std::unique_ptr<Socket> socket) {
    IO::AsyncSocket asyncSock(m_context, std::move(socket));

    while (!m_stopping.load()) {
        HTTP::Request req;
        if (!co_await read_request(asyncSock, req)) {
            break;
        }

        // Process the request (TODO: dispatch to handler)
        HTTP::Response resp;
        resp.setStatus(HTTP::StatusCode::OK);
        resp.setBody("Hello from AsyncServer!");

        co_await write_response(asyncSock, resp);
    }

    asyncSock.close();
    co_return;
}

// ============================================================================
// Request reading
// ============================================================================

Async::Task<bool> AsyncServer::read_request(IO::AsyncSocket& sock, HTTP::Request& req) {
    std::vector<uint8_t> buffer(8192);
    size_t n = co_await sock.read_some(buffer);
    if (n == 0) {
        co_return false;
    }

    // Parse the request (simplified)
    std::string raw(reinterpret_cast<char*>(buffer.data()), n);
    // TODO: proper HTTP request parsing
    (void)raw;
    (void)req;

    co_return true;
}

// ============================================================================
// Response writing
// ============================================================================

Async::Task<> AsyncServer::write_response(IO::AsyncSocket& sock, const HTTP::Response& resp) {
    auto bytes = resp.build();
    co_await sock.write_all(std::span<const uint8_t>(bytes.data(), bytes.size()));
    co_return;
}

} // namespace OmniNet::Server
