// OmniNet - HTTP Client
// v0.8.0: Phases 1-5 implemented.
//   Phase 1: types + minimal sync GET/POST
//   Phase 2: async via std::future
//   Phase 3: TLS via existing net::TLSSocket
//   Phase 4: automatic 3xx redirect following
//   Phase 5: per-request timeouts + std::stop_token cancellation
// Phases 6-8 (cookies/auth, streaming, retry, happy-eyeballs) deferred.
#ifndef OMNINET_HTTP_CLIENT_H
#define OMNINET_HTTP_CLIENT_H

#include "utils/Expected.h"
#include "HttpResponse.h"
#include "net/Socket.h"

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <stop_token>
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
// ClientConfig
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
// ParsedUrl (implementation helper, public for simplicity)
// ============================================================================

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
};

// ============================================================================
// ClientRequest
// ============================================================================

class ClientRequest {
public:
    ClientRequest& method(std::string_view m);
    const std::string& method() const { return m_method; }

    ClientRequest& url(std::string_view u);
    const std::string& url() const { return m_url; }

    ClientRequest& header(std::string_view name, std::string_view value);
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;

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
// ClientResponse
// ============================================================================

class ClientResponse {
public:
    StatusCode status() const { return m_status; }
    int statusCode() const { return static_cast<int>(m_status); }
    const std::string& reason() const { return m_reason; }
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;
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
// Client
// ============================================================================

class Client {
public:
    static Client& get_instance();

    void configure(const ClientConfig& cfg);
    const ClientConfig& config() const { return m_config; }

    // ----- Synchronous API -----

    Result<ClientResponse, ClientError>
    send_blocking(const ClientRequest& req);

    Result<ClientResponse, ClientError>
    send_blocking(const ClientRequest& req, const ClientConfig& cfg);

    Result<ClientResponse, ClientError>
    send_blocking(const ClientRequest& req, const ClientConfig& cfg,
                  const std::stop_token& stop);

    // ----- Async API (Phase 2) -----

    std::future<Result<ClientResponse, ClientError>>
    send(ClientRequest req);
    std::future<Result<ClientResponse, ClientError>>
    send(ClientRequest req, const std::stop_token& stop);
    std::future<Result<ClientResponse, ClientError>>
    send(ClientRequest req, const ClientConfig& cfg);
    std::future<Result<ClientResponse, ClientError>>
    send(ClientRequest req, const ClientConfig& cfg,
         const std::stop_token& stop);

private:
    Client() = default;

    Result<std::unique_ptr<OmniNet::Socket>, ClientError>
    connect(const std::string& scheme, const std::string& host,
            const std::string& port, const ClientConfig& cfg);

    Result<ClientResponse, ClientError>
    exchange(std::unique_ptr<OmniNet::Socket> socket,
             const ClientRequest& req, const ParsedUrl& parsed,
             const ClientConfig& cfg, const std::stop_token& stop);

    ClientConfig m_config;
};

}  // namespace OmniNet::HTTP

#endif  // OMNINET_HTTP_CLIENT_H