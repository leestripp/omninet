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
#include <unordered_set>
#include <vector>

#ifndef _WIN32
#include <unistd.h>  // pipe(), close() — POSIX self-pipe trick
#endif

#include "net/Socket.h"
#include "net/TLSSocket.h"
#include "http/Router.h"
#include "http/HttpConnection.h"
#include "admin/AdminHandler.h"

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
    
    explicit Server(Config config);
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

    /// Install process-level SIGTERM/SIGINT handlers that request a clean
    /// shutdown of the accept loop (async-signal-safe: the handler only sets
    /// a flag; the loop observes it within one poll cycle, <= 1s).
    /// Call once before start(). Without this, SIGTERM/SIGINT kill the
    /// process abruptly (e.g. `systemctl stop` hangs until SIGKILL).
    static void installSignalHandlers();
    
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
    
    // --- Lifecycle callbacks ---
    
    // v0.7.0 (Phase 11c): invoked once after the listening socket
    // is bound, before the accept loop starts. The argument is the
    // actual bound port (useful when Config::port = 0 for ephemeral
    // ports). The callback is wrapped in try/catch so a user-thrown
    // exception can't crash the accept loop.
    using ReadyCallback = std::function<void(uint16_t boundPort)>;
    void setOnReady(ReadyCallback cb);

    // v0.7.0 (prompt.txt Bug #2 + #5): post-upgrade socket handoff.
    // When a request to `path` results in a 101 response (marked via
    // Response::markProtocolSwitch), the server writes the 101 and
    // then calls `cb` with the raw socket instead of looping back to
    // read another HTTP request. The callback takes ownership of
    // the socket. Used by WebSocket::Server::on() for the
    // HTTP→WebSocket protocol switch.
    using PostUpgradeCallback =
        std::function<void(std::unique_ptr<Socket> socket,
                           const Request& req)>;

    // v0.7.0 (prompt.txt Bug #5): one struct holds both handlers
    // for a protocol-switch path. registerProtocolSwitch() uses
    // this to register both atomically under a single lock.
    struct ProtocolSwitchHandlers {
        // Called FIRST by the HTTP server. Must set status 101,
        // add Upgrade/Connection/Sec-WebSocket-Accept headers,
        // and call resp.markProtocolSwitch(). The HTTP server
        // writes the response, then invokes on_post_upgrade.
        std::function<void(const Request&, Response&)> on_upgrade;
        // Called SECOND by the HTTP server AFTER the 101 response
        // is written. Takes ownership of the socket. Must not throw.
        PostUpgradeCallback on_post_upgrade;
    };

    // v0.7.0 (prompt.txt Bug #5): register both handlers atomically.
    // This is the race-free alternative to calling any() then
    // setPostUpgradeHandler() separately. The on_upgrade handler
    // is stored so the connection loop can invoke it before the
    // Router handles the request; on_post_upgrade is invoked
    // after the 101 is written.
    void registerProtocolSwitch(std::string path, ProtocolSwitchHandlers h);

    // Deprecated since 0.7.0: use registerProtocolSwitch instead.
    // Kept for backward compatibility (calls registerProtocolSwitch).
    [[deprecated("use registerProtocolSwitch instead")]]
    void setPostUpgradeHandler(std::string path, PostUpgradeCallback cb);

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
    
    // Admin handler
    Admin::Handler& adminHandler();
    
    // --- Configuration Access ---
    
    const Config& config() const;
    uint16_t port() const;
    // v0.7.0 (Phase 11a): returns the actual port the server is bound
    // to. Useful when Config::port = 0 (OS assigns an ephemeral port)
    // and the application needs to announce the real port (e.g. for
    // service discovery, supervisors, JSON-RPC over dynamic ports).
    // Returns 0 before start(). The existing port() returns the
    // *configured* port (unchanged for backward compatibility).
    uint16_t boundPort() const;
    const std::string& address() const;
    
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
    std::mutex m_workersMutex;

    // Connection handler threads (was detached, now tracked so stop()
    // can join them before the Server is destroyed).
    // v0.9.8 leak fix: each thread carries a completion flag so finished
    // (zombie) threads can be joined and erased during normal operation —
    // an exited-but-unjoined joinable thread leaks its entire ~8 MB stack.
    struct ConnectionThread {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::vector<ConnectionThread> m_connectionThreads;
    std::mutex m_connectionThreadsMutex;

    // Joins and erases connection threads that have flagged themselves
    // finished. Caller must hold m_connectionThreadsMutex.
    void reapFinishedConnectionThreadsLocked();

    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};

    // Accept-loop lifecycle synchronization. stop() waits for runAcceptLoop()
    // to exit so no new connection threads are spawned while we join them.
    std::atomic<bool> m_acceptLoopRunning{false};
    std::mutex m_acceptLoopMutex;
    std::condition_variable m_acceptLoopCv;

    // startAsync() thread handle, joined in stop()/destructor so the
    // Server is not destroyed while the detached thread is still running.
    std::thread m_asyncThread;
    std::mutex m_asyncThreadMutex;

    // v0.7.0: self-pipe trick for instant accept-loop shutdown.
    // m_wakeupPipe[0] = read end (added to poll() in accept loop)
    // m_wakeupPipe[1] = write end (written to by stop())
    // Created in constructor, closed in destructor.
    int m_wakeupPipe[2]{-1, -1};

    std::mutex m_acceptMutex;
    std::condition_variable m_acceptCv;
    std::vector<std::unique_ptr<Socket>> m_pendingConnections;
    
    // CORS
    bool m_corsEnabled = false;
    std::string m_corsAllowedOrigins;  // raw CSV from config (kept for diagnostics)
    // v0.7.0 (prompt.txt 2026-06-19 BUG-2): parsed allowlist.
    // Parsed from m_corsAllowedOrigins in enableCORS(). "*" is a
    // special value — if present, we send "*" (allowed by the
    // CORS spec for non-credentialed requests) instead of echoing
    // the request's Origin.
    std::unordered_set<std::string> m_corsAllowedOriginSet;
    bool m_corsAllowAll = false;  // true if "*" is in the allowlist
    std::string m_corsAllowedMethods;
    std::string m_corsAllowedHeaders;
    std::chrono::seconds m_corsMaxAge;
    
    // Error handlers
    Handler m_notFoundHandler;
    Handler m_errorHandler;
    
    // v0.7.0 (Phase 11c): onReady callback
    ReadyCallback m_onReady;

    // v0.7.0 (prompt.txt Bug #5): protocol-switch registration.
    // Both the upgrade handler and the post-upgrade callback are
    // stored together under m_protocolSwitchesMutex. The
    // connection loop looks up m_protocolSwitches FIRST (before
    // the Router) — this eliminates the TOCTOU race that existed
    // when the two registrations were separate calls.
    // m_protocolSwitches is persistent (no erase) — multi-client works.
    std::mutex m_protocolSwitchesMutex;
    std::unordered_map<std::string, ProtocolSwitchHandlers> m_protocolSwitches;

    // v0.7.0 (prompt.txt Bug #2 + #5): legacy post-upgrade handoff map.
    // Kept for backward compatibility with setPostUpgradeHandler.
    // Consumed on first use (one-shot) — the old API.
    std::unordered_map<std::string, PostUpgradeCallback> m_postUpgradeHandlers;
    
    // Statistics
    Stats m_stats;
    
    // Admin handler
    std::unique_ptr<Admin::Handler> m_adminHandler;
    
    // Connection factory
    std::unique_ptr<ConnectionFactory> m_connectionFactory;

    // v0.9.0 (prompt.txt A2A streaming): registry of live per-request
    // streaming responses so stop() can force-close them and unblock
    // writer threads.
    std::mutex m_activeStreamsMutex;
    std::vector<std::weak_ptr<HTTP::StreamingResponse>> m_activeStreams;
    void registerStreamingResponse(
        const std::shared_ptr<HTTP::StreamingResponse>& stream);
};

// ============================================================================
// HTTPSServer (convenience)
// ============================================================================

class HTTPSServer : public Server {
public:
    explicit HTTPSServer(const Config& config) : Server(config) {}
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