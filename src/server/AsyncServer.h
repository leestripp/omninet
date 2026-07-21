// OmniNet - Async HTTP Server
// Non-blocking HTTP server using IoContext and AsyncSocket
#ifndef OMNINET_SERVER_ASYNC_SERVER_H
#define OMNINET_SERVER_ASYNC_SERVER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../async/AsyncHandler.h"
#include "../async/Task.h"
#include "../async/Executor.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"
#include "../http/Router.h"
#include "../io/IoContext.h"
#include "../io/AsyncSocket.h"
#include "../net/Socket.h"
#include "../thread/ThreadPool.h"

namespace OmniNet::Server {

// ============================================================================
// AsyncServer
// ============================================================================
//
// A non-blocking HTTP server that uses an event loop (IoContext) and a
// thread pool (ThreadPool) to handle many concurrent connections with
// minimal threads.
//
// Architecture:
//   - One event loop thread (IoContext) for accept and I/O multiplexing
//   - One thread pool for handler execution (CPU-bound work, DB queries)
//   - Each connection is handled by an async coroutine
//   - No thread-per-connection model
//
// Usage:
//   Thread::ThreadPool pool(4);
//   ThreadPoolExecutor exec(pool);
//   EpollIoContext ctx(exec);
//
//   AsyncServer server(ctx, 8080);
//   server.get("/hello", [](const Request& req, Async::Executor& exec)
//       -> Async::Task<HTTP::Response> {
//       Response resp;
//       resp.setBody("Hello!");
//       co_return resp;
//   });
//   server.run();
//
class AsyncServer {
public:
    // --- Configuration ---

    struct Config {
        std::string address = "0.0.0.0";
        uint16_t port = 8080;
        size_t maxConnections = 1024;
        std::chrono::seconds requestTimeout{30};
        std::chrono::seconds keepAliveTimeout{5};
        size_t maxRequestSize = 1024 * 1024;  // 1MB
        bool useNagle = true;  // TCP_NODELAY
        bool reuseAddress = true;
    };

    // --- Statistics ---

    struct Stats {
        std::atomic<uint64_t> requestsTotal{0};
        std::atomic<uint64_t> requestsActive{0};
        std::atomic<uint64_t> requestsError{0};
        std::atomic<uint64_t> connectionsTotal{0};
        std::atomic<uint64_t> connectionsActive{0};
    };

    // --- Construction ---

    AsyncServer(IO::IoContext& ctx, Config config);
    AsyncServer(IO::IoContext& ctx);  // Default config
    ~AsyncServer();

    // Non-copyable, non-movable
    AsyncServer(const AsyncServer&) = delete;
    AsyncServer& operator=(const AsyncServer&) = delete;
    AsyncServer(AsyncServer&&) = delete;
    AsyncServer& operator=(AsyncServer&&) = delete;

    // --- Routing ---

    // Register an async handler for a route pattern.
    // Pattern supports:
    //   - Exact path:     "/api/users"
    //   - Path parameter: "/api/users/:id"   (single segment)
    //   - Wildcard:       "/static/*"        (any subpath)
    void get(std::string_view pattern, Async::AsyncHandler handler);
    void post(std::string_view pattern, Async::AsyncHandler handler);
    void put(std::string_view pattern, Async::AsyncHandler handler);
    void delete_(std::string_view pattern, Async::AsyncHandler handler);
    void patch(std::string_view pattern, Async::AsyncHandler handler);
    void head(std::string_view pattern, Async::AsyncHandler handler);
    void options(std::string_view pattern, Async::AsyncHandler handler);
    void any(std::string_view pattern, Async::AsyncHandler handler);

    // Get the underlying router (for compatibility / advanced usage)
    HTTP::Router& router() { return m_router; }

    // --- Lifecycle ---

    // Start the server (blocking, returns when stopped)
    void run();

    // Start in background (non-blocking)
    void run_async();

    // Stop the server
    void stop();

    // Check if running
    bool running() const { return m_running.load(); }

    // Wait until server stops
    void wait();

    // Get statistics
    const Stats& stats() const { return m_stats; }

    // Get the IoContext
    IO::IoContext& context() { return m_context; }

private:
    // A single async route entry
    struct AsyncRoute {
        HTTP::Method method;
        std::string pattern;
        std::regex regex;
        std::vector<std::string> paramNames;
        Async::AsyncHandler handler;
    };

    // --- Internals ---

    // Convert a pattern like "/users/:id" to a regex (delegates to Router's logic)
    static std::string patternToRegex(
        const std::string& pattern,
        std::vector<std::string>& paramNames
    );

    // Add an async route
    void addRoute(HTTP::Method method, std::string pattern, Async::AsyncHandler handler);

    // Find a matching async route for the given method and path
    const AsyncRoute* findAsyncRoute(HTTP::Method method, std::string_view path);

    // Async accept loop - coroutine that lives in the event loop
    Async::Task<> accept_loop();

    // Handle a single connection (async coroutine)
    Async::Task<> handle_connection(std::unique_ptr<Socket> socket);

    // Read an HTTP request from the socket
    Async::Task<bool> read_request(IO::AsyncSocket& sock, HTTP::Request& req);

    // Write an HTTP response to the socket
    Async::Task<bool> write_response(IO::AsyncSocket& sock, const HTTP::Response& resp);

    // Spawn a coroutine and keep it alive
    void spawnTask(Async::Task<> task);

    // Configuration
    Config m_config;

    // IoContext for event loop
    IO::IoContext& m_context;

    // Router for request dispatch (kept for compatibility)
    HTTP::Router m_router;

    // Async routes
    std::vector<AsyncRoute> m_asyncRoutes;
    std::mutex m_routesMutex;

    // Listening socket
    std::unique_ptr<Socket> m_listenSocket;

    // Active coroutines (kept alive while running)
    std::mutex m_tasksMutex;
    std::vector<std::shared_ptr<Async::Task<>>> m_activeTasks;

    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    std::thread m_eventLoopThread;
    std::stop_source m_stopSource;

    // Statistics
    Stats m_stats;
};

} // namespace OmniNet::Server

#endif // OMNINET_SERVER_ASYNC_SERVER_H