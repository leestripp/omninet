// OmniNet - HTTP Request Implementation
#include "HttpRequest.h"
#include "utils/Utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace OmniNet::HTTP {
using OmniNet::Utils::toLower;
using OmniNet::Utils::trim;
using OmniNet::Utils::split;
using OmniNet::Utils::urlDecode;

// ============================================================================
// HTTP Methods
// ============================================================================

const char* toString(Method method) {
    switch (method) {
        case Method::GET:     return "GET";
        case Method::POST:    return "POST";
        case Method::PUT:     return "PUT";
        case Method::DELETE:  return "DELETE";
        case Method::PATCH:   return "PATCH";
        case Method::HEAD:    return "HEAD";
        case Method::OPTIONS: return "OPTIONS";
        case Method::CONNECT: return "CONNECT";
        case Method::TRACE:   return "TRACE";
        default:              return "UNKNOWN";
    }
}

Method parseMethod(std::string_view str) {
    if (str == "GET") return Method::GET;
    if (str == "POST") return Method::POST;
    if (str == "PUT") return Method::PUT;
    if (str == "DELETE") return Method::DELETE;
    if (str == "PATCH") return Method::PATCH;
    if (str == "HEAD") return Method::HEAD;
    if (str == "OPTIONS") return Method::OPTIONS;
    if (str == "CONNECT") return Method::CONNECT;
    if (str == "TRACE") return Method::TRACE;
    return Method::UNKNOWN;
}

// ============================================================================
// Version parsing
// ============================================================================

Version parseVersion(std::string_view str) {
    // Expected format: "HTTP/1.1" or "HTTP/2.0"
    if (str.size() >= 8 && str.substr(0, 5) == "HTTP/") {
        int major = str[5] - '0';
        int minor = str[7] - '0';
        return Version{major, minor};
    }
    return Version{1, 0};  // Default to HTTP/1.0
}

std::string versionToString(const Version& v) {
    return "HTTP/" + std::to_string(v.major) + "." + std::to_string(v.minor);
}

// ============================================================================
// Query Params
// ============================================================================

std::optional<std::string_view> QueryParams::get(std::string_view key) const {
    auto it = params.find(std::string(key));
    if (it == params.end()) return std::nullopt;
    if (auto* str = std::get_if<std::string>(&it->second)) {
        return *str;
    }
    auto* vec = std::get_if<std::vector<std::string>>(&it->second);
    if (vec && !vec->empty()) {
        return (*vec)[0];
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> QueryParams::getAll(std::string_view key) const {
    auto it = params.find(std::string(key));
    if (it == params.end()) return std::nullopt;
    if (auto* vec = std::get_if<std::vector<std::string>>(&it->second)) {
        return *vec;
    }
    if (auto* str = std::get_if<std::string>(&it->second)) {
        return std::vector<std::string>{*str};
    }
    return std::nullopt;
}

bool QueryParams::has(std::string_view key) const {
    return params.find(std::string(key)) != params.end();
}

size_t QueryParams::count(std::string_view key) const {
    auto it = params.find(std::string(key));
    if (it == params.end()) return 0;
    if (std::get_if<std::string>(&it->second)) {
        return 1;
    }
    if (auto* vec = std::get_if<std::vector<std::string>>(&it->second)) {
        return vec->size();
    }
    return 0;
}


std::optional<int> QueryParams::queryInt(std::string_view key) const {
    auto v = get(key);
    if (!v) return std::nullopt;
    try {
        size_t pos = 0;
        int n = std::stoi(std::string(*v), &pos);
        // Reject trailing non-digit chars (e.g., "10x").
        if (pos != v->size()) return std::nullopt;
        return n;
    } catch (...) {
        return std::nullopt;
    }
}

int QueryParams::queryInt(std::string_view key, int defaultValue) const {
    auto v = queryInt(key);
    return v ? *v : defaultValue;
}

std::optional<double> QueryParams::queryDouble(std::string_view key) const {
    auto v = get(key);
    if (!v) return std::nullopt;
    try {
        size_t pos = 0;
        double d = std::stod(std::string(*v), &pos);
        if (pos != v->size()) return std::nullopt;
        return d;
    } catch (...) {
        return std::nullopt;
    }
}

double QueryParams::queryDouble(std::string_view key, double defaultValue) const {
    auto v = queryDouble(key);
    return v ? *v : defaultValue;
}

// v0.8.3 (MonkeyByte Bug 3): generic string accessor.
std::optional<std::string> QueryParams::queryString(std::string_view key) const {
    auto v = get(key);  // returns optional<string_view>
    if (!v) return std::nullopt;
    return std::string(*v);  // copy out for safety
}

std::string QueryParams::queryString(std::string_view key,
                                     std::string_view defaultValue) const {
    auto v = queryString(key);
    return v ? *v : std::string(defaultValue);
}

// ============================================================================
// Request
// ============================================================================

Request::Request() = default;

size_t Request::parse(std::span<const uint8_t> data) {
    m_rawData = data;
    
    size_t pos = 0;
    const char* ptr = reinterpret_cast<const char*>(data.data());
    size_t len = data.size();
    
    // Temporary string for parsing version
    std::string versionStr;
    
    while (pos < len) {
        switch (m_state) {
            case State::METHOD: {
                // Read until space
                while (pos < len && ptr[pos] != ' ' && ptr[pos] != '\n' && ptr[pos] != '\r') {
                    m_path += ptr[pos++];
                }
                if (ptr[pos] == ' ' && m_state == State::METHOD && !m_path.empty()) {
                    m_method = parseMethod(m_path);
                    m_path.clear();
                    m_state = State::PATH;
                    ++pos; // skip space
                }
                break;
            }
            case State::PATH: {
                while (pos < len && ptr[pos] != ' ' && ptr[pos] != '\n' && ptr[pos] != '\r') {
                    m_path += ptr[pos++];
                }
                if (ptr[pos] == ' ' && m_state == State::PATH && !m_path.empty()) {
                    // Parse query string
                    auto queryPos = m_path.find('?');
                    if (queryPos != std::string::npos) {
                        m_queryString = m_path.substr(queryPos + 1);
                        m_path.resize(queryPos);
                        m_queryParsed = false;
                    }
                    m_state = State::VERSION;
                    ++pos; // skip space
                }
                break;
            }
            case State::VERSION: {
                while (pos < len && ptr[pos] != '\n' && ptr[pos] != '\r') {
                    versionStr += ptr[pos++];
                }
                if (ptr[pos] == '\n') {
                    m_version = parseVersion(versionStr);
                    m_state = State::HEADER_NAME;
                    ++pos;
                } else if (ptr[pos] == '\r' && pos + 1 < len && ptr[pos + 1] == '\n') {
                    m_version = parseVersion(versionStr);
                    m_state = State::HEADER_NAME;
                    pos += 2;
                }
                break;
            }
            case State::HEADER_NAME: {
                if (ptr[pos] == '\n' || (ptr[pos] == '\r' && pos + 1 < len && ptr[pos + 1] == '\n')) {
                    // Empty line = end of headers
                    if (m_currentHeader.empty()) {
                        if (m_contentLength > 0) {
                            m_state = State::BODY;
                            if (ptr[pos] == '\r') ++pos;
                            ++pos;
                        } else {
                            return pos + (ptr[pos] == '\r' ? 2 : 1);
                        }
                    }
                    ++pos;
                    m_currentHeader.clear();
                    continue;
                }
                if (ptr[pos] == ':') {
                    m_state = State::HEADER_VALUE;
                    ++pos;
                    // Skip optional whitespace
                    while (pos < len && (ptr[pos] == ' ' || ptr[pos] == '\t')) ++pos;
                } else {
                    m_currentHeader += ptr[pos++];
                }
                break;
            }
            case State::HEADER_VALUE: {
                if (ptr[pos] == '\n' || (ptr[pos] == '\r' && pos + 1 < len && ptr[pos + 1] == '\n')) {
                    m_headers[Utils::toLower(m_currentHeader)] = Utils::trim(m_headers[m_currentHeader]);
                    
                    // Track special headers
                    if (Utils::toLower(m_currentHeader) == "content-length") {
                        m_contentLength = std::stoul(m_headers[m_currentHeader]);
                    }
                    
                    m_state = State::HEADER_NAME;
                    ++pos;
                    if (ptr[pos - 1] == '\r' && pos < len) ++pos;
                    m_currentHeader.clear();
                } else if (ptr[pos] != '\r') {
                    m_headers[m_currentHeader] += ptr[pos++];
                } else {
                    ++pos;
                }
                break;
            }
            case State::BODY: {
                size_t remaining = len - pos;
                if (remaining >= m_contentLength) {
                    m_body.assign(data.begin() + pos, data.begin() + pos + m_contentLength);
                    return pos + m_contentLength;
                } else {
                    m_body.assign(data.begin() + pos, data.end());
                    return len;
                }
            }
        }
        
        // Advance past \n
        if (pos < len && ptr[pos] == '\n') {
            ++pos;
        }
    }
    
    return pos;
}

size_t Request::parse(std::string_view data) {
    return parse(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

bool Request::isComplete() const {
    if (m_state == State::BODY) {
        return m_body.size() >= m_contentLength;
    }
    return m_state != State::METHOD && m_state != State::PATH && 
           m_state != State::VERSION && m_state != State::HEADER_NAME &&
           m_state != State::HEADER_VALUE;
}

bool Request::hasBody() const {
    return m_contentLength > 0;
}

std::span<const uint8_t> Request::rawData() const {
    return m_rawData;
}

Method Request::method() const {
    return m_method;
}

std::string_view Request::methodString() const {
    return toString(m_method);
}

std::string_view Request::path() const {
    return m_path;
}

std::string_view Request::queryString() const {
    return m_queryString;
}

const QueryParams& Request::query() const {
    if (!m_queryParsed) {
        const_cast<Request*>(this)->parseQueryString();
    }
    return m_queryParams;
}

// v0.8.3 (MonkeyByte Bug 3): convenience accessors.
std::optional<std::string> Request::queryParameter(std::string_view key) const {
    return query().queryString(key);
}

std::string Request::queryParameter(std::string_view key,
                                    std::string_view defaultValue) const {
    return query().queryString(key, defaultValue);
}

Version Request::version() const {
    return m_version;
}

std::string_view Request::pathParam(size_t index) const {
    // v1.0.2 fix: indexed params were retrieved by advancing an iterator
    // into the unordered_map, so pathParam(0..N) came back in unspecified
    // (hash) order — e.g. /api/v1/users/456 returned 456 for index 0.
    // Look up the numeric key directly instead.
    auto it = m_pathParams.find(std::to_string(index));
    if (it != m_pathParams.end()) {
        return it->second;
    }
    return "";
}

std::string_view Request::pathParam(std::string_view name) const {
    auto it = m_pathParams.find(std::string(name));
    if (it != m_pathParams.end()) {
        return it->second;
    }
    return "";
}

void Request::setPathParams(const std::vector<std::string>& params) {
    m_pathParams.clear();
    for (size_t i = 0; i < params.size(); ++i) {
        m_pathParams[std::to_string(i)] = params[i];
    }
}

void Request::setPathParam(std::string_view name, std::string_view value) {
    m_pathParams[std::string(name)] = std::string(value);
}

std::optional<std::string_view> Request::header(std::string_view name) const {
    auto it = m_headers.find(Utils::toLower(std::string(name)));
    if (it != m_headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string_view Request::headerOr(std::string_view name, std::string_view defaultValue) const {
    auto result = header(name);
    return result.value_or(defaultValue);
}

bool Request::hasHeader(std::string_view name) const {
    return m_headers.find(Utils::toLower(std::string(name))) != m_headers.end();
}

std::string_view Request::contentType() const {
    return headerOr("content-type", "");
}

size_t Request::contentLength() const {
    return m_contentLength;
}

std::string_view Request::host() const {
    return headerOr("host", "");
}

std::string_view Request::userAgent() const {
    return headerOr("user-agent", "");
}

std::string_view Request::referer() const {
    return headerOr("referer", "");
}

// v0.7.0 (Phase 11d): header helpers for auth + tracing.
// Thin wrappers around header(); return empty view if absent.

std::string_view Request::authHeader() const {
    return headerOr("Authorization", "");
}

std::string_view Request::bearerToken() const {
    // Authorization: Bearer <token>
    auto auth = authHeader();
    static constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() > kBearer.size() &&
        (auth.substr(0, kBearer.size()) == kBearer)) {
        return auth.substr(kBearer.size());
    }
    return {};
}

std::string_view Request::traceId() const {
    return headerOr("X-Trace-Id", "");
}

std::string_view Request::requestId() const {
    return headerOr("X-Request-Id", "");
}

bool Request::keepAlive() const {
    auto conn = headerOr("connection", "");
    std::string lc = Utils::toLower(conn);
    // Explicit Connection header takes precedence
    if (lc == "close") return false;
    if (lc == "keep-alive") return true;
    // Default based on HTTP version
    return version().isHTTP11();
}

bool Request::isChunked() const {
    return headerOr("transfer-encoding", "") == "chunked";
}

std::string_view Request::transferEncoding() const {
    return headerOr("transfer-encoding", "");
}

std::span<const uint8_t> Request::body() const {
    return m_body;
}

std::string_view Request::bodyAsString() const {
    return std::string_view(reinterpret_cast<const char*>(m_body.data()), m_body.size());
}

std::optional<QueryParams> Request::parseFormBody() const {
    if (contentType().find("application/x-www-form-urlencoded") != 0) {
        return std::nullopt;
    }
    
    QueryParams params;
    auto bodyStr = bodyAsString();
    auto pairs = Utils::split(bodyStr, '&');
    
    for (const auto& pair : pairs) {
        auto kv = Utils::split(pair, '=');
        if (kv.size() >= 1) {
            auto key = Utils::urlDecode(kv[0]);
            if (kv.size() >= 2) {
                params.params[key] = Utils::urlDecode(kv[1]);
            } else {
                params.params[key] = std::string();
            }
        }
    }
    
    return params;
}

std::string_view Request::remoteAddress() const {
    return m_remoteAddress;
}

void Request::setRemoteAddress(std::string_view addr) {
    m_remoteAddress = std::string(addr);
}

uint16_t Request::remotePort() const {
    return m_remotePort;
}

void Request::setRemotePort(uint16_t port) {
    m_remotePort = port;
}

std::string Request::url() const {
    std::string result;
    // Note: Would need scheme from context
    result += path();
    if (!m_queryString.empty()) {
        result += '?';
        result += m_queryString;
    }
    return result;
}

void Request::reset() {
    m_method = Method::GET;
    m_version = HTTP_1_1;
    m_path.clear();
    m_queryString.clear();
    m_headers.clear();
    m_body.clear();
    m_pathParams.clear();
    m_contentLength = 0;
    m_state = State::METHOD;
    m_currentHeader.clear();
    m_queryParsed = false;
    m_remoteAddress.clear();
    m_remotePort = 0;
}

void Request::parseQueryString() {
    if (m_queryParsed) return;
    
    if (!m_queryString.empty()) {
        auto pairs = Utils::split(m_queryString, '&');
        for (const auto& pair : pairs) {
            auto kv = Utils::split(pair, '=');
            if (kv.size() >= 1) {
                auto key = Utils::urlDecode(kv[0]);
                if (kv.size() >= 2) {
                    m_queryParams.params[key] = Utils::urlDecode(kv[1]);
                } else {
                    m_queryParams.params[key] = std::string();
                }
            }
        }
    }
    
    m_queryParsed = true;
}

// ============================================================================
// Setters
// ============================================================================

void Request::setMethod(Method method) {
    m_method = method;
}

void Request::setPath(std::string path) {
    m_path = std::move(path);
}

void Request::setVersion(Version version) {
    m_version = version;
}

void Request::setHeader(std::string name, std::string value) {
    m_headers[std::move(name)] = std::move(value);
}

void Request::setBody(std::vector<uint8_t> data) {
    m_body = std::move(data);
}

// ============================================================================
// Request Builder
// ============================================================================

RequestBuilder& RequestBuilder::setMethod(Method method) {
    m_request.setMethod(method);
    return *this;
}

RequestBuilder& RequestBuilder::setPath(std::string_view path) {
    m_request.setPath(std::string(path));
    return *this;
}

RequestBuilder& RequestBuilder::setVersion(Version version) {
    m_request.setVersion(version);
    return *this;
}

RequestBuilder& RequestBuilder::setHeader(std::string_view name, std::string_view value) {
    m_request.setHeader(std::string(name), std::string(value));
    return *this;
}

RequestBuilder& RequestBuilder::setBody(std::span<const uint8_t> data) {
    std::vector<uint8_t> bodyData(data.begin(), data.end());
    m_request.setBody(std::move(bodyData));
    return *this;
}

RequestBuilder& RequestBuilder::setBody(std::string_view data) {
    std::vector<uint8_t> bodyData(data.begin(), data.end());
    m_request.setBody(std::move(bodyData));
    return *this;
}

RequestBuilder& RequestBuilder::setContentType(std::string_view type) {
    m_request.setHeader("Content-Type", std::string(type));
    return *this;
}

Request RequestBuilder::build() {
    return std::move(m_request);
}

} // namespace OmniNet::HTTP
