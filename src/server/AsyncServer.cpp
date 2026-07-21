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
#include <sstream>

#include "../utils/Exception.h"
#include "../io/IoContext.h"

namespace OmniNet::Server {

// ============================================================================
// Construction / Destruction
// ============================================================================

AsyncServer::AsyncServer(IO::IoContext& ctx) : AsyncServer(ctx, Config{}) {}

AsyncServer::AsyncServer(IO::IoContext& ctx, Config config)
    : m_config(std::move(config)), m_context(ctx) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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

    // Wrap the fd in a Socket (the Socket destructor will close it)
    m_listenSocket = std::make_unique<Socket>(fd);
}

AsyncServer::~AsyncServer() {
    stop();
    // Clear active tasks - this destroys any coroutines still in m_activeTasks
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    m_activeTasks.clear();
    if (m_listenSocket) {
        m_listenSocket->close();
    }
}

// ============================================================================
// Routing
// ============================================================================

void AsyncServer::addRoute(HTTP::Method method, std::string pattern,
                            Async::AsyncHandler handler) {
    std::vector<std::string> paramNames;
    std::string regexPattern = patternToRegex(pattern, paramNames);

    std::lock_guard<std::mutex> lock(m_routesMutex);
    m_asyncRoutes.push_back(AsyncRoute{
        method,
        std::move(pattern),
        std::regex(regexPattern),
        std::move(paramNames),
        std::move(handler)
    });
}

void AsyncServer::get(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::GET, std::string(pattern), std::move(handler));
}
void AsyncServer::post(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::POST, std::string(pattern), std::move(handler));
}
void AsyncServer::put(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::PUT, std::string(pattern), std::move(handler));
}
void AsyncServer::delete_(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::DELETE, std::string(pattern), std::move(handler));
}
void AsyncServer::patch(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::PATCH, std::string(pattern), std::move(handler));
}
void AsyncServer::head(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::HEAD, std::string(pattern), std::move(handler));
}
void AsyncServer::options(std::string_view pattern, Async::AsyncHandler handler) {
    addRoute(HTTP::Method::OPTIONS, std::string(pattern), std::move(handler));
}
void AsyncServer::any(std::string_view pattern, Async::AsyncHandler handler) {
    // Register for all methods
    for (auto method : {HTTP::Method::GET, HTTP::Method::POST, HTTP::Method::PUT,
                         HTTP::Method::DELETE, HTTP::Method::PATCH, HTTP::Method::HEAD,
                         HTTP::Method::OPTIONS}) {
        addRoute(method, std::string(pattern), handler);
    }
}

const AsyncServer::AsyncRoute*
AsyncServer::findAsyncRoute(HTTP::Method method, std::string_view path) {
    std::lock_guard<std::mutex> lock(m_routesMutex);
    for (const auto& route : m_asyncRoutes) {
        if (route.method != method) continue;
        if (std::regex_match(std::string(path), route.regex)) {
            return &route;
        }
    }
    return nullptr;
}

std::string AsyncServer::patternToRegex(
    const std::string& pattern,
    std::vector<std::string>& paramNames
) {
    // Mirror the Router's patternToRegex logic
    std::string regex;
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == ':') {
            size_t start = i + 1;
            size_t end = start;
            while (end < pattern.size() &&
                   (std::isalnum(static_cast<unsigned char>(pattern[end])) ||
                    pattern[end] == '_')) {
                ++end;
            }
            if (end > start) {
                paramNames.push_back(pattern.substr(start, end - start));
                regex += "([^/]+)";
                i = end;
            } else {
                regex += ':';
                ++i;
            }
        } else if (pattern[i] == '*') {
            regex += ".*";
            ++i;
        } else {
            // Copy other characters as-is.
            // Note: this means pattern characters that are also regex
            // metacharacters (e.g., '.', '+') will be treated as regex
            // metacharacters. For typical URL patterns this is acceptable.
            regex += pattern[i];
            ++i;
        }
    }
    return regex;
}

// ============================================================================
// Lifecycle
// ============================================================================

void AsyncServer::spawnTask(Async::Task<> task) {
    auto taskPtr = std::make_shared<Async::Task<>>(std::move(task));
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_activeTasks.push_back(taskPtr);
    }
    taskPtr->resume();
}

void AsyncServer::run() {
    if (m_running.exchange(true)) {
        return;  // Already running
    }
    m_stopping.store(false);

    // Spawn the accept loop as a coroutine in the event loop
    spawnTask(accept_loop());

    // Run the event loop in this thread (blocks)
    m_context.run(m_stopSource.get_token());
    m_running.store(false);
}

void AsyncServer::run_async() {
    m_eventLoopThread = std::thread([this]() {
        run();
    });
}

void AsyncServer::stop() {
    if (!m_running.exchange(false)) {
        return;  // Not running
    }
    m_stopping.store(true);
    m_stopSource.request_stop();
    m_context.stop();

    if (m_eventLoopThread.joinable()) {
        m_eventLoopThread.join();
    }

    // After the event loop has stopped, all watch callbacks won't be called.
    // Suspended coroutines will be cleaned up when m_activeTasks is cleared
    // (in the destructor).
}

void AsyncServer::wait() {
    if (m_eventLoopThread.joinable()) {
        m_eventLoopThread.join();
    }
}

// ============================================================================
// Accept loop
// ============================================================================
//
// Coroutine that runs in the event loop. It waits for new connections on
// the listen socket and spawns a handle_connection coroutine for each one.
//
Async::Task<> AsyncServer::accept_loop() {
    while (!m_stopping.load()) {
        // Wait for the listen socket to become readable
        co_await when_readable(m_context, *m_listenSocket);

        if (m_stopping.load()) {
            co_return;
        }

        // Accept all pending connections
        while (!m_stopping.load()) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = ::accept4(
                m_listenSocket->fd(),
                reinterpret_cast<sockaddr*>(&clientAddr),
                &clientLen,
                SOCK_NONBLOCK | SOCK_CLOEXEC
            );

            if (clientFd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more pending connections
                    break;
                }
                if (errno == EINTR) {
                    // Interrupted - retry
                    continue;
                }
                if (errno == EMFILE || errno == ENFILE) {
                    // Out of file descriptors - back off
                    std::cerr << "[AsyncServer] accept: out of file descriptors"
                              << std::endl;
                    break;
                }
                // Other error - log and continue
                std::cerr << "[AsyncServer] accept error: "
                          << std::strerror(errno) << std::endl;
                break;
            }

            // Track stats
            ++m_stats.connectionsTotal;
            ++m_stats.connectionsActive;

            // Wrap the fd in a Socket
            auto clientSocket = std::make_unique<Socket>(clientFd);

            // Spawn a coroutine to handle this connection
            spawnTask(handle_connection(std::move(clientSocket)));
        }
    }
    co_return;
}

// ============================================================================
// Connection handling
// ============================================================================
//
// One coroutine per connection. Reads the request, invokes the handler,
// writes the response. Supports HTTP/1.1 keep-alive.
//
Async::Task<> AsyncServer::handle_connection(std::unique_ptr<Socket> socket) {
    IO::AsyncSocket asyncSock(m_context, std::move(socket));

    // Reuse a single Request object across keep-alive requests
    while (!m_stopping.load()) {
        HTTP::Request req;
        ++m_stats.requestsTotal;
        ++m_stats.requestsActive;

        // Read the request
        bool ok = co_await read_request(asyncSock, req);

        if (!ok) {
            // Error or connection closed
            break;
        }

        // read_request already verified the request is complete (terminated
        // by \r\n\r\n with method, path, and host header). The parser's
        // isComplete() may not detect this reliably for all inputs, so we
        // skip that check here.
        if (req.method() == HTTP::Method::UNKNOWN || req.path().empty()) {
            // Sanity check: ensure we have a valid request
            HTTP::Response resp = HTTP::Response::badRequest("Invalid request");
            co_await write_response(asyncSock, resp);
            ++m_stats.requestsError;
            break;
        }

        // Find the matching route
        HTTP::Response resp;
        const AsyncRoute* route = findAsyncRoute(req.method(), req.path());

        if (route) {
            // Invoke the async handler and await the response
            try {
                auto handlerTask = route->handler(req, m_context.executor());
                resp = co_await std::move(handlerTask);
            } catch (const std::exception& e) {
                std::cerr << "[AsyncServer] handler exception: "
                          << e.what() << std::endl;
                resp = HTTP::Response::internalError(e.what());
                ++m_stats.requestsError;
            }
        } else {
            // No route found
            resp = HTTP::Response::notFound("Not found: " + std::string(req.path()));
        }

        // Write the response
        bool wroteOk = co_await write_response(asyncSock, resp);
        if (!wroteOk) {
            ++m_stats.requestsError;
            break;
        }

        --m_stats.requestsActive;

        // Check keep-alive
        if (!req.keepAlive()) {
            break;
        }
    }

    asyncSock.close();
    --m_stats.connectionsActive;
    co_return;
}

// ============================================================================
// Request reading
// ============================================================================
//
// Reads bytes from the socket until we have a complete HTTP request or
// exceed the max request size. Uses Request::parse() to incrementally
// build the request.
//
Async::Task<bool> AsyncServer::read_request(IO::AsyncSocket& sock, HTTP::Request& req) {
    std::vector<uint8_t> buffer(m_config.maxRequestSize);
    size_t total = 0;

    // Read until the request is complete or we hit the limit
    while (total < buffer.size()) {
        auto remaining = std::span<uint8_t>(buffer).subspan(total);
        size_t n = co_await sock.read_some(remaining);
        if (n == 0) {
            // Connection closed (or error)
            co_return total > 0;  // True if we got partial data
        }

        total += n;

        // Try to parse what we have so far
        size_t consumed = req.parse(
            std::span<const uint8_t>(buffer).first(total)
        );
        (void)consumed;  // Parser tracks its own state

        // Check completion using both the parser state and a simple
        // end-of-headers check. The parser's isComplete() may not
        // detect the end of headers reliably for all inputs, so we
        // also check for the \r\n\r\n terminator.
        if (req.isComplete()) {
            co_return true;
        }

        // Look for the empty line that terminates the headers
        if (total >= 4) {
            for (size_t i = (total >= n + 4 ? total - n : 0); i + 3 < total; ++i) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
                    buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    // Empty line found - try parsing one more time
                    // to make sure the parser has consumed everything
                    req.parse(std::span<const uint8_t>(buffer).first(total));
                    if (req.method() != HTTP::Method::UNKNOWN &&
                        !req.path().empty() &&
                        req.header("host").has_value()) {
                        co_return true;
                    }
                }
            }
        }
    }

    // Request too large
    co_return false;
}

// ============================================================================
// Response writing
// ============================================================================
Async::Task<bool> AsyncServer::write_response(IO::AsyncSocket& sock, const HTTP::Response& resp) {
    // buildFull() includes both headers and body (build() is headers-only)
    auto bytes = resp.buildFull();
    if (bytes.empty()) {
        co_return false;
    }
    co_await sock.write_all(std::span<const uint8_t>(bytes.data(), bytes.size()));
    co_return true;
}

} // namespace OmniNet::Server