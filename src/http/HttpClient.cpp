// OmniNet - HTTP Client Implementation (v0.8.0 Phase 1)
//
// Phase 1 scope:
//   - GET and POST only
//   - Plain OmniNet::Socket transport only (TLS in Phase 3)
//   - Single connection per request (no pool, no keep-alive)
//   - Synchronous only (async std::future in Phase 2)
//   - No redirect following (Phase 4)
//   - No streaming (Phase 7)
//   - No cancellation / stop_token (Phase 5)
//   - No cookies / auth helpers (Phase 6)

#include "http/HttpClient.h"

#include "net/Socket.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
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

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;   // includes leading '/' and any query string
};

Result<ParsedUrl, ClientError> parseUrl(const std::string& url) {
    ParsedUrl out;
    const std::string sep = "://";
    auto pos = url.find(sep);
    if (pos == std::string::npos) {
        return std::unexpected(ClientError::InvalidUrl);
    }
    out.scheme = url.substr(0, pos);
    for (auto& c : out.scheme) c = static_cast<char>(std::tolower(c));

    if (out.scheme == "https") {
        return std::unexpected(ClientError::UnsupportedProtocol);
    }
    if (out.scheme != "http") {
        return std::unexpected(ClientError::InvalidUrl);
    }

    size_t host_start = pos + sep.size();
    auto path_pos = url.find('/', host_start);
    std::string hostport;
    if (path_pos == std::string::npos) {
        hostport = url.substr(host_start);
        out.path = "/";
    } else {
        hostport = url.substr(host_start, path_pos - host_start);
        out.path = url.substr(path_pos);
    }

    auto colon = hostport.find(':');
    if (colon == std::string::npos) {
        out.host = hostport;
        out.port = "80";
    } else {
        out.host = hostport.substr(0, colon);
        out.port = hostport.substr(colon + 1);
    }
    if (out.host.empty() || out.port.empty()) {
        return std::unexpected(ClientError::InvalidUrl);
    }
    return out;
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(c));
    return out;
}

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
            }
        }
        pos = next_eol + 2;
    }

    if (pos < raw.size()) {
        size_t available = raw.size() - pos;
        size_t to_copy = have_content_length
            ? std::min(content_length, available)
            : available;
        std::vector<uint8_t> body(raw.begin() + pos,
                                  raw.begin() + pos + to_copy);
        resp.setBody(std::move(body));
    }
    return resp;
}

// Read all bytes from the socket until EOF (server closes).
// Phase 5: add timeout via poll().
std::string readAll(OmniNet::Socket& socket) {
    std::string out;
    std::vector<uint8_t> chunk(64 * 1024);
    while (true) {
        // OmniNet::Socket::recvAll returns the bytes received this batch.
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

Result<ClientResponse, ClientError>
Client::send_blocking(const ClientRequest& req) {
    return send_blocking(req, m_config);
}

Result<ClientResponse, ClientError>
Client::send_blocking(const ClientRequest& req, const ClientConfig& cfg) {
    // 1. Parse URL.
    auto parsed = parseUrl(req.url());
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }
    if (parsed->scheme != "http") {
        return std::unexpected(ClientError::UnsupportedProtocol);
    }

    // 2. Connect.
    auto sock_result = connect(parsed->scheme, parsed->host,
                               parsed->port, cfg);
    if (!sock_result.has_value()) {
        return std::unexpected(sock_result.error());
    }

    // 3. Send + receive.
    return exchange(std::move(sock_result.value()), req, cfg);
}

Result<std::unique_ptr<OmniNet::Socket>, ClientError>
Client::connect(const std::string& /*scheme*/, const std::string& host,
                const std::string& port, const ClientConfig& /*cfg*/) {
    try {
        auto sock = std::make_unique<OmniNet::Socket>();
        uint16_t port_num = 0;
        for (char c : port) {
            if (c < '0' || c > '9') {
                return std::unexpected(ClientError::InvalidUrl);
            }
            port_num = static_cast<uint16_t>(port_num * 10 + (c - '0'));
        }
        sock->connect(host, port_num);
        if (!sock->isOpen()) {
            return std::unexpected(ClientError::ConnectFailed);
        }
        return sock;
    } catch (...) {
        return std::unexpected(ClientError::ConnectFailed);
    }
}

Result<ClientResponse, ClientError>
Client::exchange(std::unique_ptr<OmniNet::Socket> socket,
                 const ClientRequest& req, const ClientConfig& cfg) {
    // Re-parse URL inside exchange (called from redirect logic in Phase 4).
    auto parsed = parseUrl(req.url());
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    // Build the wire format with placeholders for Host / User-Agent.
    std::ostringstream os;
    os << req.method() << ' ' << parsed->path << " HTTP/1.1\r\n";
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
    std::string wire = os.str();
    if (!req.bodyBytes().empty()) {
        wire.append(reinterpret_cast<const char*>(req.bodyBytes().data()),
                    req.bodyBytes().size());
    }

    // Replace placeholders with real values.
    const std::string host_header =
        parsed->port == "80" ? parsed->host
                             : (parsed->host + ":" + parsed->port);
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

    // Send.
    try {
        std::span<const uint8_t> out(
            reinterpret_cast<const uint8_t*>(wire.data()), wire.size());
        socket->sendAll(out);
    } catch (...) {
        return std::unexpected(ClientError::SendFailed);
    }

    // Receive.
    std::string raw;
    try {
        raw = readAll(*socket);
    } catch (...) {
        return std::unexpected(ClientError::RecvFailed);
    }

    return parseResponse(raw);
}

}  // namespace OmniNet::HTTP