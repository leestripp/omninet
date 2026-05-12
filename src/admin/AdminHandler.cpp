// OmniNet - Admin Handler Implementation
#include "admin/AdminHandler.h"
#include "http/HttpServer.h"

#include <chrono>
#include <ctime>
#include <sstream>

namespace OmniNet::Admin {

// Shared version string - should match tools_common version
constexpr const char* SHARED_VERSION = "0.1.0";

Handler::Handler() {
    m_startTime = std::chrono::steady_clock::now();
}

void Handler::mount(HTTP::Server& server) {
    // Register admin endpoints
    server.get("/omninet/health", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleHealth(req, resp);
    });
    
    server.get("/omninet/stats", [this](const HTTP::Request& req, HTTP::Response& resp) {
        handleStats(req, resp);
    });
}

void Handler::recordRequest(bool success, int64_t bytesReceived, int64_t bytesSent) {
    m_requestsTotal++;
    if (success) {
        m_requestsSuccess++;
    } else {
        m_requestsError++;
    }
    if (bytesReceived > 0) {
        m_bytesReceived += bytesReceived;
    }
    if (bytesSent > 0) {
        m_bytesSent += bytesSent;
    }
}

void Handler::recordConnectionOpen() {
    m_connectionsTotal++;
    m_connectionsActive++;
}

void Handler::recordConnectionClose() {
    m_connectionsActive--;
}

Handler::Stats Handler::getStats() const {
    Stats stats;
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_startTime);
    stats.requestsTotal = m_requestsTotal.load();
    stats.requestsSuccess = m_requestsSuccess.load();
    stats.requestsError = m_requestsError.load();
    stats.bytesReceived = m_bytesReceived.load();
    stats.bytesSent = m_bytesSent.load();
    stats.connectionsTotal = m_connectionsTotal.load();
    stats.connectionsActive = m_connectionsActive.load();
    stats.timestamp = std::chrono::system_clock::now();
    return stats;
}

std::string Handler::version() {
    return SHARED_VERSION;
}

void Handler::handleHealth(const HTTP::Request& req, HTTP::Response& resp) {
    (void)req;
    
    auto stats = getStats();
    
    std::ostringstream body;
    body << "{\n";
    body << "  \"status\": \"ok\",\n";
    body << "  \"uptime\": " << stats.uptime.count() << ",\n";
    body << "  \"version\": \"" << version() << "\",\n";
    body << "  \"requests\": " << stats.requestsTotal << ",\n";
    body << "  \"connections\": " << stats.connectionsActive << ",\n";
    
    // Timestamp
    auto time = std::chrono::system_clock::to_time_t(stats.timestamp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "\"%Y-%m-%dT%H:%M:%SZ\"", std::gmtime(&time));
    body << "  \"timestamp\": " << buf << "\n";
    
    body << "}\n";
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(body.str());
}

void Handler::handleStats(const HTTP::Request& req, HTTP::Response& resp) {
    (void)req;
    
    auto stats = getStats();
    
    std::ostringstream body;
    body << "{\n";
    body << "  \"requests\": {\n";
    body << "    \"total\": " << stats.requestsTotal << ",\n";
    body << "    \"success\": " << stats.requestsSuccess << ",\n";
    body << "    \"errors\": " << stats.requestsError << "\n";
    body << "  },\n";
    body << "  \"connections\": {\n";
    body << "    \"active\": " << stats.connectionsActive << ",\n";
    body << "    \"total\": " << stats.connectionsTotal << "\n";
    body << "  },\n";
    body << "  \"bytes\": {\n";
    body << "    \"received\": " << stats.bytesReceived << ",\n";
    body << "    \"sent\": " << stats.bytesSent << "\n";
    body << "  },\n";
    body << "  \"uptime\": " << stats.uptime.count() << ",\n";
    
    // Timestamp
    auto time = std::chrono::system_clock::to_time_t(stats.timestamp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "\"%Y-%m-%dT%H:%M:%SZ\"", std::gmtime(&time));
    body << "  \"timestamp\": " << buf << "\n";
    
    body << "}\n";
    
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(body.str());
}

} // namespace OmniNet::Admin
