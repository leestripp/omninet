// OmniNet - HTTP Client Implementation (v0.8.0)
// Phases 1-5: types + sync send + TLS + async + redirects + timeouts/cancellation.

#include "http/HttpClient.h"

#include "net/Socket.h"
#include "net/TLSSocket.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace OmniNet::HTTP {

// ============================================================================
// ClientError
// ============================================================================

const char* clientErrorName(ClientError e) {
    switch (e) {
        case ClientError::None:                return "None";
        case ClientError::InvalidUrl:          return "InvalidUrl";
        case ClientError::ConnectFailed:       return "ConnectFailed";
        case ClientError::SendFailed:          return "SendFailed";
        case ClientError::RecvFailed:          return "RecvFailed";
        case ClientError::Timeout:             return "Timeout";
        case ClientError::ParseError:          return "ParseError";
        case ClientError::TooManyRedirects:    return "TooManyRedirects";
        case ClientError::Cancelled:           return "Cancelled";
        case ClientError::TlsHandshakeFailed:  return "TlsHandshakeFailed";
        case ClientError::TlsVerifyFailed:     return "TlsVerifyFailed";
        case ClientError::UnsupportedProtocol: return "UnsupportedProtocol";
        case ClientError::Internal:            return "Internal";
    }
    return "Unknown";
}

// ============================================================================
// Helpers (anonymous namespace)
// ============================================================================

namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(c));
    return out;
}

Result<ParsedUrl, ClientError> parseUrl(const std::string& url) {
    ParsedUrl out;
    const std::string sep = "://";
    auto pos = url.find(sep);
    if (pos == std::string::npos) {
        return std::unexpected(ClientError::InvalidUrl);
    }
    out.scheme = url.substr(0, pos);
    for (auto& c : out.scheme) c = static_cast<char>(std::tolower(c));

    if (out.scheme != "http" && out.scheme != "https") {
        return std::unexpected(ClientError::InvalidUrl);
    }

    size_t host_start = pos + sep.size();
    auto path_pos = url.find('/', host_start);
    std::string hostport;
    if (path_pos == std::string::npos) {
        hostport = url.substr(host_start);
        out.path = "/";
        out.query = "";
    } else {
        hostport = url.substr(host_start, path_pos - host_start);
        auto query_pos = url.find('?', path_pos);
        if (query_pos == std::string::npos) {
            out.path = url.substr(path_pos);
            out.query = "";
        } else {
            out.path = url.substr(path_pos, query_pos - path_pos);
            out.query = url.substr(query_pos + 1);
        }
    }

    auto colon = hostport.find(':');
    if (colon == std::string::npos) {
        out.host = hostport;
        out.port = (out.scheme == "https") ? "443" : "80";
    } else {
        out.host = hostport.substr(0, colon);
        out.port = hostport.substr(colon + 1);
    }
    if (out.host.empty() || out.port.empty()) {
        return std::unexpected(ClientError::InvalidUrl);
    }
    return out;
}

std::string buildPath(const ParsedUrl& u) {
    if (u.query.empty()) return u.path;
    return u.path + "?" + u.query;
}

// Parse a single HTTP/1.x response.
Result<ClientResponse, ClientError> parseResponse(const std::string& raw) {
    ClientResponse resp;
    auto eol = raw.find("\r\n");
    if (eol == std::string::npos) {
        return std::unexpected(ClientError::ParseError);
    }
    std::string status_line = raw.substr(0, eol);
    if (status_line.compare(0, 5, "HTTP/") != 0) {
        return std::unexpected(ClientError::ParseError);
    }
    auto sp1 = status_line.find(' ');
    if (sp1 == std::string::npos) {
        return std::unexpected(ClientError::ParseError);
    }
    auto sp2 = status_line.find(' ', sp1 + 1);
    std::string code_str;
    std::string reason;
    if (sp2 == std::string::npos) {
        code_str = status_line.substr(sp1 + 1);
        reason = "";
    } else {
        code_str = status_line.substr(sp1 + 1, sp2 - sp1 - 1);
        reason = status_line.substr(sp2 + 1);
    }
    int code = 0;
    for (char c : code_str) {
        if (c < '0' || c > '9') {
            return std::unexpected(ClientError::ParseError);
        }
        code = code * 10 + (c - '0');
    }
    resp.setStatus(static_cast<StatusCode>(code), reason);

    size_t pos = eol + 2;
    size_t content_length = 0;
    bool have_content_length = false;
    bool chunked = false;
    while (pos < raw.size()) {
        auto next_eol = raw.find("\r\n", pos);
        if (next_eol == std::string::npos) break;
        if (next_eol == pos) {
            pos = next_eol + 2;
            break;
        }
        std::string header_line = raw.substr(pos, next_eol - pos);
        auto colon = header_line.find(':');
        if (colon != std::string::npos) {
            std::string name = header_line.substr(0, colon);
            size_t v = colon + 1;
            while (v < header_line.size() &&
                   (header_line[v] == ' ' || header_line[v] == '\t')) ++v;
            std::string value = header_line.substr(v);
            while (!value.empty() && (value.back() == '\r' ||
                   value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            resp.addHeader(name, value);
            std::string lower_name = toLower(name);
            if (lower_name == "content-length") {
                content_length = 0;
                for (char c : value) {
                    if (c < '0' || c > '9') { content_length = 0; break; }
                    content_length = content_length * 10 + (c - '0');
                }
                have_content_length = true;
            } else if (lower_name == "transfer-encoding") {
                std::string lv = toLower(value);
                if (lv.find("chunked") != std::string::npos) chunked = true;
            }
        }
        pos = next_eol + 2;
    }

    if (chunked) {
        std::vector<uint8_t> body;
        while (pos < raw.size()) {
            auto chunk_eol = raw.find("\r\n", pos);
            if (chunk_eol == std::string::npos) break;
            std::string size_line = raw.substr(pos, chunk_eol - pos);
            auto semi = size_line.find(';');
            if (semi != std::string::npos) size_line.resize(semi);
            size_t chunk_size = 0;
            for (char c : size_line) {
                int d = (c >= '0' && c <= '9') ? c - '0'
                       : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                       : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                if (d < 0) break;
                chunk_size = chunk_size * 16 + static_cast<size_t>(d);
            }
            pos = chunk_eol + 2;
            if (chunk_size == 0) {
                if (pos < raw.size() && raw[pos] == '\r') ++pos;
                if (pos < raw.size() && raw[pos] == '\n') ++pos;
                break;
            }
            if (pos + chunk_size > raw.size()) break;
            body.insert(body.end(), raw.begin() + pos,
                        raw.begin() + pos + chunk_size);
            pos += chunk_size;
            if (pos < raw.size() && raw[pos] == '\r') ++pos;
            if (pos < raw.size() && raw[pos] == '\n') ++pos;
        }
        resp.setBody(std::move(body));
    } else if (pos < raw.size()) {
        size_t available = raw.size() - pos;
        size_t to_copy = have_content_length
            ? std::min(content_length, available) : available;
        std::vector<uint8_t> body(raw.begin() + pos,
                                  raw.begin() + pos + to_copy);
        resp.setBody(std::move(body));
    }
    return resp;
}

std::string buildRequest(const ClientRequest& req, const std::string& path) {
    std::ostringstream os;
    os << req.method() << ' ' << path << " HTTP/1.1\r\n";
    os << "Host: <set>\r\n";
    os << "User-Agent: <set>\r\n";
    os << "Accept: */*\r\n";
    os << "Connection: close\r\n";
    for (const auto& [k, v] : req.headers()) {
        os << k << ": " << v << "\r\n";
    }
    if (!req.bodyBytes().empty()) {
        os << "Content-Length: " << req.bodyBytes().size() << "\r\n";
    }
    os << "\r\n";
    std::string out = os.str();
    if (!req.bodyBytes().empty()) {
        out.append(reinterpret_cast<const char*>(req.bodyBytes().data()),
                   req.bodyBytes().size());
    }
    return out;
}

std::string readAll(OmniNet::Socket& socket, const ClientConfig& cfg,
                    const std::stop_token& stop) {
    std::string out;
    std::vector<uint8_t> chunk(64 * 1024);
    auto deadline = std::chrono::steady_clock::now() + cfg.request_timeout;
    bool use_timeout = cfg.request_timeout.count() > 0;
    bool check_stop = stop.stop_possible();

    while (true) {
        if (check_stop && stop.stop_requested()) break;

        auto now = std::chrono::steady_clock::now();
        if (use_timeout && now >= deadline) break;

        auto poll = std::chrono::milliseconds(50);
        if (use_timeout) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now);
            poll = std::min(remaining, poll);
        }
        if (!socket.waitForRead(poll)) continue;

        auto got = socket.recvAll(chunk.size());
        if (got.empty()) break;
        out.append(reinterpret_cast<const char*>(got.data()), got.size());
    }
    return out;
}

}  // namespace

// ============================================================================
// ClientRequest
// ============================================================================

ClientRequest& ClientRequest::method(std::string_view m) {
    m_method = std::string(m);
    return *this;
}

ClientRequest& ClientRequest::url(std::string_view u) {
    m_url = std::string(u);
    return *this;
}

ClientRequest& ClientRequest::header(std::string_view name,
                                    std::string_view value) {
    m_headers[toLower(name)] = std::string(value);
    return *this;
}

std::string ClientRequest::header(std::string_view name) const {
    auto lower = toLower(name);
    auto it = m_headers.find(lower);
    if (it != m_headers.end()) return it->second;
    return {};
}

ClientRequest& ClientRequest::body(std::string_view data) {
    m_body.assign(data.begin(), data.end());
    return *this;
}

ClientRequest& ClientRequest::body(std::vector<uint8_t> data) {
    m_body = std::move(data);
    return *this;
}

std::string_view ClientRequest::bodyString() const {
    return std::string_view(reinterpret_cast<const char*>(m_body.data()),
                             m_body.size());
}

// ============================================================================
// ClientResponse
// ============================================================================

void ClientResponse::setStatus(StatusCode code, std::string_view reason) {
    m_status = code;
    m_reason = std::string(reason);
}

void ClientResponse::addHeader(std::string_view name,
                              std::string_view value) {
    m_headers[toLower(name)] = std::string(value);
}

std::string ClientResponse::header(std::string_view name) const {
    auto lower = toLower(name);
    auto it = m_headers.find(lower);
    if (it != m_headers.end()) return it->second;
    return {};
}

void ClientResponse::setBody(std::vector<uint8_t> data) {
    m_body = std::move(data);
}

std::string ClientResponse::bodyAsString() const {
    return std::string(reinterpret_cast<const char*>(m_body.data()),
                       m_body.size());
}

std::string_view ClientResponse::bodyAsStringView() const {
    return std::string_view(reinterpret_cast<const char*>(m_body.data()),
                             m_body.size());
}

// ============================================================================
// Client
// ============================================================================

Client& Client::get_instance() {
    static Client instance;
    return instance;
}

void Client::configure(const ClientConfig& cfg) {
    m_config = cfg;
}

// ----- Synchronous API -----

Result<ClientResponse, ClientError>
Client::send_blocking(const ClientRequest& req) {
    return send_blocking(req, m_config, std::stop_token{});
}

Result<ClientResponse, ClientError>
Client::send_blocking(const ClientRequest& req, const ClientConfig& cfg) {
    return send_blocking(req, cfg, std::stop_token{});
}

Result<ClientResponse, ClientError>
Client::send_blocking(const ClientRequest& req, const ClientConfig& cfg,
                      const std::stop_token& stop) {
    ClientRequest current = req;
    for (int hop = 0; hop <= cfg.max_redirect_hops; ++hop) {
        auto parsed = parseUrl(current.url());
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        auto sock_result = connect(parsed->scheme, parsed->host,
                                   parsed->port, cfg);
        if (!sock_result.has_value()) {
            return std::unexpected(sock_result.error());
        }
        auto result = exchange(std::move(sock_result.value()),
                               current, *parsed, cfg, stop);
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        int sc = result->statusCode();
        if (sc < 300 || sc >= 400) return result;
        if (sc == 304) return result;
        if (!cfg.follow_redirects) return result;
        std::string loc = result->header("Location");
        if (loc.empty()) return result;
        if (hop == cfg.max_redirect_hops) {
            return std::unexpected(ClientError::TooManyRedirects);
        }
        ClientRequest next = current;
        next.url(loc);
        if (sc == 301 || sc == 302 || sc == 303) {
            next.method("GET");
            std::vector<uint8_t> empty;
            next.body(std::move(empty));
        }
        current = next;
    }
    return std::unexpected(ClientError::TooManyRedirects);
}

// ----- Async API (Phase 2) -----

std::future<Result<ClientResponse, ClientError>>
Client::send(ClientRequest req) {
    ClientConfig cfg = m_config;
    Client* self = this;
    return std::async(std::launch::async,
                      [req = std::move(req), cfg, self]() mutable {
        return self->send_blocking(req, cfg, std::stop_token{});
    });
}

std::future<Result<ClientResponse, ClientError>>
Client::send(ClientRequest req, const std::stop_token& stop) {
    ClientConfig cfg = m_config;
    Client* self = this;
    return std::async(std::launch::async,
                      [req = std::move(req), cfg, stop, self]() mutable {
        return self->send_blocking(req, cfg, stop);
    });
}

std::future<Result<ClientResponse, ClientError>>
Client::send(ClientRequest req, const ClientConfig& cfg) {
    Client* self = this;
    return std::async(std::launch::async,
                      [req = std::move(req), cfg, self]() {
        return self->send_blocking(req, cfg, std::stop_token{});
    });
}

std::future<Result<ClientResponse, ClientError>>
Client::send(ClientRequest req, const ClientConfig& cfg,
             const std::stop_token& stop) {
    Client* self = this;
    return std::async(std::launch::async,
                      [req = std::move(req), cfg, stop, self]() mutable {
        return self->send_blocking(req, cfg, stop);
    });
}

// ----- Internal: connect (TLS-aware) -----

Result<std::unique_ptr<OmniNet::Socket>, ClientError>
Client::connect(const std::string& scheme, const std::string& host,
                const std::string& port, const ClientConfig& cfg) {
    uint16_t port_num = 0;
    for (char c : port) {
        if (c < '0' || c > '9') {
            return std::unexpected(ClientError::InvalidUrl);
        }
        port_num = static_cast<uint16_t>(port_num * 10 + (c - '0'));
    }
    try {
        if (scheme == "https") {
            auto tlsCtx = OmniNet::TLSContext::createClient();
            auto sock = tlsCtx->createSocket();
            sock->setSNIHostname(host);
            sock->setALPNProtocols({"http/1.1"});
            sock->connect(host, port_num);
            if (!sock->isOpen()) {
                return std::unexpected(ClientError::ConnectFailed);
            }
            if (!cfg.verify_tls) {
                auto* tls = dynamic_cast<OmniNet::TLSSocket*>(sock.get());
                if (tls) tls->setVerifyMode(OmniNet::TLSVerify::NONE);
            }
            return sock;
        } else {
            auto sock = std::make_unique<OmniNet::Socket>();
            sock->connect(host, port_num);
            if (!sock->isOpen()) {
                return std::unexpected(ClientError::ConnectFailed);
            }
            return sock;
        }
    } catch (...) {
        return std::unexpected(ClientError::ConnectFailed);
    }
}

// ----- Internal: exchange (send + recv) -----

Result<ClientResponse, ClientError>
Client::exchange(std::unique_ptr<OmniNet::Socket> socket,
                 const ClientRequest& req, const ParsedUrl& parsed,
                 const ClientConfig& cfg, const std::stop_token& stop) {
    std::string path = buildPath(parsed);
    std::string wire = buildRequest(req, path);

    const std::string host_header =
        ((parsed.scheme == "http" && parsed.port == "80") ||
         (parsed.scheme == "https" && parsed.port == "443"))
        ? parsed.host : (parsed.host + ":" + parsed.port);
    const std::string host_line = "Host: " + host_header + "\r\n";
    const std::string ua_line = "User-Agent: " + cfg.user_agent + "\r\n";
    auto host_pos = wire.find("Host: <set>\r\n");
    if (host_pos != std::string::npos) {
        wire.replace(host_pos, std::string("Host: <set>\r\n").size(),
                     host_line);
    }
    auto ua_pos = wire.find("User-Agent: <set>\r\n");
    if (ua_pos != std::string::npos) {
        wire.replace(ua_pos, std::string("User-Agent: <set>\r\n").size(),
                     ua_line);
    }

    try {
        std::span<const uint8_t> out(
            reinterpret_cast<const uint8_t*>(wire.data()), wire.size());
        socket->sendAll(out);
    } catch (...) {
        return std::unexpected(ClientError::SendFailed);
    }

    std::string raw;
    try {
        raw = readAll(*socket, cfg, stop);
        if (stop.stop_possible() && stop.stop_requested()) {
            return std::unexpected(ClientError::Cancelled);
        }
    } catch (...) {
        return std::unexpected(ClientError::RecvFailed);
    }

    return parseResponse(raw);
}

}  // namespace OmniNet::HTTP
