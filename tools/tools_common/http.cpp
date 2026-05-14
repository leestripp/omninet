// OmniNet Tools - HTTP Client Implementation
#include "http.h"

#include <cstring>
#include <sstream>
#include <regex>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

namespace OmniNet::Tools {

// ============================================================================
// HTTP Client Implementation
// ============================================================================

HttpClient::HttpClient() = default;

HttpClient::~HttpClient() = default;

HttpClient& HttpClient::timeout(std::chrono::seconds timeout) {
    m_timeout = timeout;
    return *this;
}

HttpClient& HttpClient::followRedirects(bool follow) {
    m_followRedirects = follow;
    return *this;
}

HttpClient& HttpClient::maxRedirects(int max) {
    m_maxRedirects = max;
    return *this;
}

HttpClient& HttpClient::verifySSL(bool verify) {
    m_verifySSL = verify;
    return *this;
}

HttpClient& HttpClient::header(const std::string& name, const std::string& value) {
    m_headers[name] = value;
    return *this;
}

HttpClient& HttpClient::headers(const std::map<std::string, std::string>& headers) {
    m_headers = headers;
    return *this;
}

void HttpClient::clearHeaders() {
    m_headers.clear();
}

std::optional<HttpResponse> HttpClient::get(const std::string& url) {
    return request("GET", url);
}

std::optional<HttpResponse> HttpClient::post(const std::string& url,
                                              const std::string& body,
                                              const std::string& contentType) {
    return request("POST", url, body, contentType);
}

std::optional<HttpResponse> HttpClient::head(const std::string& url) {
    return request("HEAD", url);
}

bool HttpClient::parseUrl(const std::string& url,
                          std::string& scheme,
                          std::string& host,
                          int& port,
                          std::string& path) {
    // Simple URL parser
    // Supports: http://host:port/path, https://host:port/path
    
    static std::regex urlRegex(R"(^(https?)://([^:/]+)(?::(\d+))?(/.*)?$)");
    std::smatch match;
    
    if (!std::regex_match(url, match, urlRegex)) {
        m_lastError = "Invalid URL format";
        return false;
    }
    
    scheme = match[1];
    host = match[2];
    port = match[3].matched ? std::stoi(match[3]) : (scheme == "https" ? 443 : 80);
    std::string pathStr = match[4].str();
    path = (!pathStr.empty() || match[4].matched) ? pathStr : "/";
    
    return true;
}

std::optional<HttpResponse> HttpClient::request(const std::string& method,
                                                const std::string& url,
                                                const std::string& body,
                                                const std::string& contentType) {
    // Parse URL
    std::string scheme, host, path;
    int port;
    
    if (!parseUrl(url, scheme, host, port, path)) {
        return std::nullopt;
    }
    
    // For HTTPS, we would use OpenSSL - simplified to HTTP only for now
    if (scheme == "https") {
        m_lastError = "HTTPS not implemented in simple client";
        return std::nullopt;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        m_lastError = "WSAStartup failed";
        return std::nullopt;
    }
#endif
    
    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        m_lastError = "Failed to create socket";
#ifdef _WIN32
        WSACleanup();
#endif
        return std::nullopt;
    }
    
    // Set timeout
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(m_timeout.count() * 1000);
#else
    struct timeval tv;
    tv.tv_sec = m_timeout.count();
    tv.tv_usec = 0;
#endif
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    
    // Resolve host
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        m_lastError = "Failed to resolve host";
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return std::nullopt;
    }
    
    // Connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        m_lastError = "Failed to connect to server";
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return std::nullopt;
    }
    
    // Build request
    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    
    for (const auto& h : m_headers) {
        req << h.first << ": " << h.second << "\r\n";
    }
    
    if (!contentType.empty()) {
        req << "Content-Type: " << contentType << "\r\n";
    }
    
    if (!body.empty()) {
        req << "Content-Length: " << body.length() << "\r\n";
    }
    
    req << "Connection: close\r\n";
    req << "\r\n";
    
    if (!body.empty()) {
        req << body;
    }
    
    // Send request
    std::string reqStr = req.str();
    if (send(sock, reqStr.c_str(), static_cast<int>(reqStr.length()), 0) < 0) {
        m_lastError = "Failed to send request";
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return std::nullopt;
    }
    
    // Receive response
    std::string response;
    char buffer[4096];
    
    while (true) {
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        response += buffer;
    }
    
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    
    auto endTime = std::chrono::steady_clock::now();
    
    // Parse response
    HttpResponse resp;
    resp.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Parse status line
    size_t pos = response.find("\r\n");
    if (pos == std::string::npos) {
        m_lastError = "Invalid HTTP response";
        return std::nullopt;
    }
    
    std::string statusLine = response.substr(0, pos);
    static std::regex statusRegex(R"(HTTP/1\.\d\s+(\d+)\s*(.*))");
    std::smatch match;
    
    if (std::regex_match(statusLine, match, statusRegex)) {
        resp.statusCode = std::stoi(match[1]);
        resp.statusText = match[2];
    } else {
        m_lastError = "Failed to parse status line";
        return std::nullopt;
    }
    
    // Parse headers
    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        std::string headersStr = response.substr(pos + 2, headerEnd - pos - 2);
        
        std::istringstream headerStream(headersStr);
        std::string headerLine;
        while (std::getline(headerStream, headerLine)) {
            if (headerLine.back() == '\r') {
                headerLine.pop_back();
            }
            size_t colonPos = headerLine.find(':');
            if (colonPos != std::string::npos) {
                std::string name = headerLine.substr(0, colonPos);
                std::string value = headerLine.substr(colonPos + 1);
                // Trim whitespace
                while (value.length() > 0 && value[0] == ' ') {
                    value = value.substr(1);
                }
                resp.headers[name] = value;
            }
        }
        
        resp.body = response.substr(headerEnd + 4);
    }
    
    return resp;
}

// ============================================================================
// HealthChecker Implementation
// ============================================================================

HealthChecker::HealthChecker() = default;

HealthChecker::HealthChecker(const std::string& baseUrl) : m_baseUrl(baseUrl) {}

HealthChecker& HealthChecker::timeout(std::chrono::seconds timeout) {
    m_timeout = timeout;
    m_client.timeout(timeout);
    return *this;
}

HealthChecker& HealthChecker::verifySSL(bool verify) {
    m_verifySSL = verify;
    m_client.verifySSL(verify);
    return *this;
}

HealthCheckResult HealthChecker::check(const std::string& endpoint) {
    return check(endpoint, {});
}

HealthCheckResult HealthChecker::check(const std::string& endpoint, const std::string& expectedBody) {
    HealthCheckResult result;
    result.endpoint = endpoint;
    
    auto url = m_baseUrl + endpoint;
    if (endpoint.empty() || endpoint[0] != '/') {
        url = m_baseUrl + "/" + endpoint;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    auto response = m_client.get(url);
    auto endTime = std::chrono::steady_clock::now();
    
    result.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (!response) {
        result.healthy = false;
        result.error = m_client.lastError();
        return result;
    }
    
    result.statusCode = response->statusCode;
    result.responseBody = response->body;
    
    // Consider 2xx as healthy
    result.healthy = (result.statusCode >= 200 && result.statusCode < 300);
    
    // Check expected body if provided
    if (!expectedBody.empty() && result.healthy) {
        if (result.responseBody.find(expectedBody) == std::string::npos) {
            result.healthy = false;
            result.error = "Response body does not contain expected content";
        }
    }
    
    return result;
}

HealthReport HealthChecker::checkAll(const std::vector<std::string>& endpoints) {
    HealthReport report;
    report.target = m_baseUrl;
    report.checkedAt = std::chrono::system_clock::now();
    
    for (const auto& endpoint : endpoints) {
        report.checks.push_back(check(endpoint));
    }
    
    report.allHealthy = true;
    for (const auto& check : report.checks) {
        if (!check.healthy) {
            report.allHealthy = false;
            break;
        }
    }
    
    return report;
}

std::vector<std::string> HealthChecker::defaultEndpoints() {
    return {
        "/omninet/health"
    };
}

} // namespace OmniNet::Tools
