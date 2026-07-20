// OmniNet - HTTP Client
// v0.8.0: Phases 1-5 implemented.
//   Phase 1: types + minimal sync GET/POST
//   Phase 2: async via std::future
//   Phase 3: TLS via existing net::TLSSocket
//   Phase 4: automatic 3xx redirect following
//   Phase 5: per-request timeouts + std::stop_token cancellation
//   Phase 7: streaming bodies (request via set_body_writer;
//             response via ClientResponseStream::read_some())
// Phases 6 and 8 (auth/cookies, retry/happy-eyeballs) deferred.
#ifndef OMNINET_HTTP_CLIENT_H
#define OMNINET_HTTP_CLIENT_H

#include "utils/Expected.h"
#include "HttpResponse.h"
#include "net/Socket.h"

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
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
    bool verify_tls = true;
};

// ============================================================================
// ParsedUrl (implementation helper)
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
    using BodyWriter = std::function<std::vector<uint8_t>()>;

    ClientRequest& method(std::string_view m);
    const std::string& method() const { return m_method; }

    ClientRequest& url(std::string_view u);
    const std::string& url() const { return m_url; }

    ClientRequest& header(std::string_view name, std::string_view value);
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;

    // Buffer the full body in memory (Phase 1).
    ClientRequest& body(std::string_view data);
    ClientRequest& body(std::vector<uint8_t> data);
    const std::vector<uint8_t>& bodyBytes() const { return m_body; }
    std::string_view bodyString() const;

    // Stream the body via a callback (Phase 7). Returns next chunk;
    // empty vector signals end-of-body. Uses Transfer-Encoding: chunked.
    ClientRequest& set_body_writer(BodyWriter writer);
    bool has_body_writer() const { return m_body_writer != nullptr; }

private:
    friend class Client;
    std::string m_method = "GET";
    std::string m_url;
    std::map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
    BodyWriter m_body_writer;
};

// ============================================================================
// ClientResponse (Phase 1-5: full body in memory)
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
// ClientResponseStream (Phase 7) — incremental body access
// ============================================================================

class ClientResponseStream {
public:
    ClientResponseStream(StatusCode status, std::string reason,
                        std::map<std::string, std::string> headers,
                        std::unique_ptr<struct ResponseStreamState> state);

    ~ClientResponseStream();

    ClientResponseStream(const ClientResponseStream&) = delete;
    ClientResponseStream& operator=(const ClientResponseStream&) = delete;

    StatusCode status() const { return m_status; }
    int statusCode() const { return static_cast<int>(m_status); }
    const std::string& reason() const { return m_reason; }
    const std::map<std::string, std::string>& headers() const { return m_headers; }
    std::string header(std::string_view name) const;

    // Pull next chunk from the wire. empty optional = end of body.
    std::optional<std::vector<uint8_t>> read_some();

    // Convenience: collect all remaining bytes into one vector.
    std::vector<uint8_t> read_all();

    bool eof() const;
    bool error() const { return m_error; }

private:
    StatusCode m_status;
    std::string m_reason;
    std::map<std::string, std::string> m_headers;
    std::unique_ptr<struct ResponseStreamState> m_state;
    bool m_error = false;
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

    // ----- Streaming API (Phase 7) -----

    Result<std::unique_ptr<ClientResponseStream>, ClientError>
    send_streaming(ClientRequest req, const ClientConfig& cfg);
    Result<std::unique_ptr<ClientResponseStream>, ClientError>
    send_streaming(ClientRequest req);

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