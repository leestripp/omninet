// OmniNet - HTTP Client
// v0.8.0 (Phase 1): minimal sync client. Plain Socket transport,
// no pool, no redirect, no TLS, no async. Subsequent phases
// (see CodeMax.md) add these capabilities.
#ifndef OMNINET_HTTP_CLIENT_H
#define OMNINET_HTTP_CLIENT_H
#include "utils/Expected.h"
#include "HttpResponse.h"
#include "net/Socket.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OmniNet::HTTP {

// ============================================================================
// ClientError
// ============================================================================

enum class ClientError {
    None = 0,
    InvalidUrl,
    ConnectFailed,
    SendFailed,
    RecvFailed,
    Timeout,
    ParseError,
    TooManyRedirects,
    Cancelled,
    TlsHandshakeFailed,
    TlsVerifyFailed,
    UnsupportedProtocol,
    Internal,
};

const char* clientErrorName(ClientError e);

// ============================================================================
// ClientConfig (Phase 1: minimal)
// ============================================================================

struct ClientConfig {
    std::string user_agent = "OmniNet-HttpClient/0.8";
    std::chrono::seconds connect_timeout{10};
    std::chrono::seconds request_timeout{60};
    bool follow_redirects = true;
    int  max_redirect_hops = 5;
    bool verify_tls = true;   // used in Phase 3
};

// ============================================================================
// ClientRequest (Phase 1: minimal)
// ============================================================================

class ClientRequest {
public:
    // Set the request method (GET, POST, etc.). Defaults to GET.
    ClientRequest& method(std::string_view m);
    const std::string& method() const { return m_method; }

    // Set the request URL. Must include scheme + host + path.
    // e.g., "http://example.com/foo?bar=1"
    ClientRequest& url(std::string_view u);
    const std::string& url() const { return m_url; }

    // Add a request header. Multi-value: call multiple times with same name.
    ClientRequest& header(std::string_view name, std::string_view value);
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;  // case-insensitive lookup

    // Set the request body (Content-Length implied).
    ClientRequest& body(std::string_view data);
    ClientRequest& body(std::vector<uint8_t> data);
    const std::vector<uint8_t>& bodyBytes() const { return m_body; }
    std::string_view bodyString() const;

private:
    std::string m_method = "GET";
    std::string m_url;
    std::map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
};

// ============================================================================
// ClientResponse (Phase 1: minimal)
// ============================================================================

class ClientResponse {
public:
    StatusCode status() const { return m_status; }
    int statusCode() const { return static_cast<int>(m_status); }
    const std::string& reason() const { return m_reason; }
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;  // case-insensitive lookup
    const std::vector<uint8_t>& body() const { return m_body; }
    std::string bodyAsString() const;
    std::string_view bodyAsStringView() const;

    // Internal: used by Client
    void setStatus(StatusCode code, std::string_view reason = {});
    void addHeader(std::string_view name, std::string_view value);
    void setBody(std::vector<uint8_t> data);

private:
    StatusCode m_status = StatusCode::OK;
    std::string m_reason;
    std::map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
};

// ============================================================================
// Client (Phase 1: minimal sync singleton)
// ============================================================================

class Client {
public:
    static Client& get_instance();

    // Configure globally (singleton). Subsequent sends use this config
    // unless overridden via send_blocking(req, config).
    void configure(const ClientConfig& cfg);
    const ClientConfig& config() const { return m_config; }

    // Synchronous request. Blocks until response or error.
    // Returns the response on success, ClientError on failure.
    // Phase 1: plain Socket only (no TLS), no redirect, no streaming.
    Result<ClientResponse, ClientError>
    send_blocking(const ClientRequest& req);

    // Same as send_blocking but uses an explicit config (does not mutate
    // the global configure() state).
    Result<ClientResponse, ClientError>
    send_blocking(const ClientRequest& req, const ClientConfig& cfg);

private:
    Client() = default;

    // Internal: connect to host:port, returns owned Socket on success.
    // Phase 1: only "http://" scheme; returns InvalidUrl for "https://".
    Result<std::unique_ptr<OmniNet::Socket>, ClientError>
    connect(const std::string& scheme, const std::string& host,
            const std::string& port, const ClientConfig& cfg);

    // Internal: send request and read full response.
    Result<ClientResponse, ClientError>
    exchange(std::unique_ptr<OmniNet::Socket> socket,
             const ClientRequest& req, const ClientConfig& cfg);

    ClientConfig m_config;
};

}  // namespace OmniNet::HTTP

#endif  // OMNINET_HTTP_CLIENT_H