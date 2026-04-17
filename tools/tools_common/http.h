// OmniNet Tools - HTTP Client for Health Checks
#ifndef OMNINET_TOOLS_HTTP_H
#define OMNINET_TOOLS_HTTP_H

#include <string>
#include <optional>
#include <chrono>
#include <variant>
#include <vector>
#include <map>

namespace OmniNet::Tools {

// ============================================================================
// HTTP Response
// ============================================================================

struct HttpResponse {
    int statusCode = 0;
    std::string statusText;
    std::string body;
    std::map<std::string, std::string> headers;
    std::chrono::milliseconds responseTime{0};
};

// ============================================================================
// HTTP Client
// ============================================================================

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    // Configuration
    HttpClient& timeout(std::chrono::seconds timeout);
    HttpClient& followRedirects(bool follow);
    HttpClient& maxRedirects(int max);
    HttpClient& verifySSL(bool verify);
    
    // GET request
    std::optional<HttpResponse> get(const std::string& url);
    
    // POST request
    std::optional<HttpResponse> post(const std::string& url, 
                                     const std::string& body = {},
                                     const std::string& contentType = "application/json");
    
    // HEAD request
    std::optional<HttpResponse> head(const std::string& url);
    
    // Set custom headers
    HttpClient& header(const std::string& name, const std::string& value);
    HttpClient& headers(const std::map<std::string, std::string>& headers);
    
    // Clear custom headers
    void clearHeaders();
    
    // Get last error
    const std::string& lastError() const { return m_lastError; }

private:
    std::chrono::seconds m_timeout{10};
    bool m_followRedirects = true;
    int m_maxRedirects = 5;
    bool m_verifySSL = true;
    std::map<std::string, std::string> m_headers;
    std::string m_lastError;
    
    std::optional<HttpResponse> request(const std::string& method, 
                                        const std::string& url,
                                        const std::string& body = {},
                                        const std::string& contentType = {});
    
    bool parseUrl(const std::string& url, 
                  std::string& scheme,
                  std::string& host,
                  int& port,
                  std::string& path);
};

// ============================================================================
// Health Check
// ============================================================================

struct HealthCheckResult {
    std::string endpoint;
    bool healthy = false;
    int statusCode = 0;
    std::string error;
    std::chrono::milliseconds responseTime{0};
    std::string responseBody;
};

struct HealthReport {
    std::string target;
    std::vector<HealthCheckResult> checks;
    bool allHealthy = false;
    std::chrono::system_clock::time_point checkedAt;
};

class HealthChecker {
public:
    HealthChecker();
    explicit HealthChecker(const std::string& baseUrl);
    
    // Configure
    HealthChecker& timeout(std::chrono::seconds timeout);
    HealthChecker& verifySSL(bool verify);
    
    // Check single endpoint
    HealthCheckResult check(const std::string& endpoint);
    HealthCheckResult check(const std::string& endpoint, const std::string& expectedBody);
    
    // Check multiple endpoints
    HealthReport checkAll(const std::vector<std::string>& endpoints);
    
    // Built-in health endpoints
    static std::vector<std::string> defaultEndpoints();
    
    // Set base URL
    void setBaseUrl(const std::string& url) { m_baseUrl = url; }
    std::string baseUrl() const { return m_baseUrl; }

private:
    std::string m_baseUrl;
    std::chrono::seconds m_timeout{10};
    bool m_verifySSL = true;
    HttpClient m_client;
};

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_HTTP_H
