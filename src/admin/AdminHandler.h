// OmniNet - Admin Handler
// Built-in admin endpoints under /omninet/*

#ifndef OMNINET_ADMIN_HANDLER_H
#define OMNINET_ADMIN_HANDLER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace OmniNet::HTTP {
class Server;
}

namespace OmniNet::Admin {

// ============================================================================
// Admin Handler - Built-in admin endpoints
// ============================================================================

class Handler {
public:
    Handler();
    
    // Mount endpoints on HTTP server
    void mount(HTTP::Server& server);
    
    // Stats recording (called from HttpServer)
    void recordRequest(bool success, int64_t bytesReceived, int64_t bytesSent);
    void recordConnectionOpen();
    void recordConnectionClose();
    
    // Get stats snapshot (for /omninet/stats endpoint)
    struct Stats {
        std::chrono::seconds uptime{0};
        int64_t requestsTotal{0};
        int64_t requestsSuccess{0};
        int64_t requestsError{0};
        int64_t bytesReceived{0};
        int64_t bytesSent{0};
        int64_t connectionsTotal{0};
        int64_t connectionsActive{0};
        std::chrono::system_clock::time_point timestamp;
    };
    
    Stats getStats() const;
    
    // Version info
    static std::string version();
    
private:
    // Route handlers
    void handleHealth(const HTTP::Request& req, HTTP::Response& resp);
    void handleStats(const HTTP::Request& req, HTTP::Response& resp);
    
    // Stats tracking
    std::atomic<int64_t> m_requestsTotal{0};
    std::atomic<int64_t> m_requestsSuccess{0};
    std::atomic<int64_t> m_requestsError{0};
    std::atomic<int64_t> m_bytesReceived{0};
    std::atomic<int64_t> m_bytesSent{0};
    std::atomic<int64_t> m_connectionsTotal{0};
    std::atomic<int64_t> m_connectionsActive{0};
    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace OmniNet::Admin

#endif // OMNINET_ADMIN_HANDLER_H
