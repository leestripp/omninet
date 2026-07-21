// OmniNet - HTTP Request
#ifndef OMNINET_HTTP_REQUEST_H
#define OMNINET_HTTP_REQUEST_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace OmniNet::HTTP {

// ============================================================================
// HTTP Methods
// ============================================================================

enum class Method {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS,
    CONNECT,
    TRACE,
    UNKNOWN
};

const char* toString(Method method);
Method parseMethod(std::string_view str);

// ============================================================================
// HTTP Version
// ============================================================================

struct Version {
    int major = 1;
    int minor = 1;
    
    bool isHTTP11() const { return major == 1 && minor == 1; }
    bool isHTTP10() const { return major == 1 && minor == 0; }
    bool isHTTP20() const { return major == 2; }
};

constexpr Version HTTP_1_0{1, 0};
constexpr Version HTTP_1_1{1, 1};
constexpr Version HTTP_2{2, 0};

// ============================================================================
// Query Parameters
// ============================================================================

using QueryParamValue = std::variant<std::string, std::vector<std::string>>;

struct QueryParams {
    std::unordered_map<std::string, QueryParamValue> params;
    
    std::optional<std::string_view> get(std::string_view key) const;
    std::optional<std::vector<std::string>> getAll(std::string_view key) const;
    bool has(std::string_view key) const;
    size_t count(std::string_view key) const;

    // v0.7.7 (ApolloCore Bug 4): typed accessors so users don't
    // have to std::string(view) before std::stoi/std::stod.
    // queryInt/queryDouble return std::optional<T> - nullopt if
    // the key is absent OR the value cannot be parsed as a number.
    // The default-value overloads return the default if the key
    // is absent (and on malformed, to match the 'best-effort'
    // semantic of URL params).
    std::optional<int> queryInt(std::string_view key) const;
    int queryInt(std::string_view key, int defaultValue) const;
    std::optional<double> queryDouble(std::string_view key) const;
    double queryDouble(std::string_view key, double defaultValue) const;

    // v0.8.3 (MonkeyByte Bug 3): generic string accessor that
    // returns an OWNED std::string. Complements queryInt/queryDouble
    // (which return numbers) and `get()` (which returns a view into
    // the params map and can dangle if the map is moved).
    // - queryString(key)           -> std::optional<std::string>,
    //                                nullopt if key absent.
    // - queryString(key, default)  -> std::string, default if absent.
    // For multi-value params (e.g., ?k=a&k=b), returns the first value
    // (use getAll() to retrieve all values).
    std::optional<std::string> queryString(std::string_view key) const;
    std::string                queryString(std::string_view key,
                                           std::string_view defaultValue) const;
};

// ============================================================================
// HTTP Request
// ============================================================================

class Request {
public:
    Request();
    
    // --- Parsing ---
    
    // Parse from buffer (returns bytes consumed)
    size_t parse(std::span<const uint8_t> data);
    
    // Parse from string
    size_t parse(std::string_view data);
    
    // Check if request is complete
    bool isComplete() const;
    
    // Check if request has body
    bool hasBody() const;
    
    // --- Raw Data Access ---
    
    // Get raw request data
    std::span<const uint8_t> rawData() const;
    
    // --- Request Line ---
    
    Method method() const;
    std::string_view methodString() const;
    
    std::string_view path() const;
    std::string_view queryString() const;
    const QueryParams& query() const;

    // v0.8.3 (MonkeyByte Bug 3): convenience accessors that delegate
    // to QueryParams::queryString(). Returns an OWNED std::string.
    // Use these instead of `query().get()` when you need a value
    // you can safely hold past the request lifetime.
    std::optional<std::string> queryParameter(std::string_view key) const;
    std::string                queryParameter(std::string_view key,
                                              std::string_view defaultValue) const;
    
    Version version() const;
    
    // Path parameters (for parameterized routes like /users/:id)
    std::string_view pathParam(size_t index) const;
    std::string_view pathParam(std::string_view name) const;
    void setPathParams(const std::vector<std::string>& params);
    void setPathParam(std::string_view name, std::string_view value);
    
    // --- Headers ---
    
    std::optional<std::string_view> header(std::string_view name) const;
    std::string_view headerOr(std::string_view name, std::string_view defaultValue) const;
    bool hasHeader(std::string_view name) const;
    
    // Common headers
    std::string_view contentType() const;
    size_t contentLength() const;
    std::string_view host() const;
    std::string_view userAgent() const;
    std::string_view referer() const;

    // v0.7.0 (Phase 11d): header helpers for auth + tracing.
    // Thin wrappers around header(); return empty view if absent.

    // Raw Authorization header (e.g. "Bearer xxx" or "Basic xxx").
    std::string_view authHeader() const;

    // Bearer token (strips "Bearer " prefix); empty if no Bearer scheme.
    std::string_view bearerToken() const;

    // X-Trace-Id, used for distributed tracing.
    std::string_view traceId() const;

    // X-Request-Id, per-request correlation id.
    std::string_view requestId() const;
    
    // --- Connection ---
    
    bool keepAlive() const;
    bool isChunked() const;
    
    // Get transfer encoding value
    std::string_view transferEncoding() const;
    
    // --- Body ---
    
    std::span<const uint8_t> body() const;
    std::string_view bodyAsString() const;
    
    // Parse body as form data (application/x-www-form-urlencoded)
    std::optional<QueryParams> parseFormBody() const;
    
    // Parse body as JSON (requires JSON header)
    // Note: Basic JSON parsing without external dependency
    // For full JSON support, use nlohmann/json or similar
    
    // --- Remote Info ---
    
    std::string_view remoteAddress() const;
    void setRemoteAddress(std::string_view addr);
    
    uint16_t remotePort() const;
    void setRemotePort(uint16_t port);
    
    // --- Utility ---
    
    // Get full URL (scheme://host:port/path?query)
    std::string url() const;
    
    // --- Setters (for RequestBuilder) ---
    void setMethod(Method method);
    void setPath(std::string path);
    void setVersion(Version version);
    void setHeader(std::string name, std::string value);
    void setBody(std::vector<uint8_t> data);
    
    // Reset for reuse
    void reset();

private:
    bool parseRequestLine(std::string_view line);
    bool parseHeaderLine(std::string_view line);
    bool parseBody();
    
    Method m_method = Method::GET;
    Version m_version{HTTP_1_1};
    std::string m_path;
    std::string m_queryString;
    std::unordered_map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
    std::unordered_map<std::string, std::string> m_pathParams;
    
    // Parsing state
    enum class State {
        METHOD,
        PATH,
        VERSION,
        HEADER_NAME,
        HEADER_VALUE,
        BODY
    };
    State m_state = State::METHOD;
    std::string m_currentHeader;
    size_t m_contentLength = 0;
    
    // Remote info (set by connection handler)
    std::string m_remoteAddress;
    uint16_t m_remotePort = 0;
    
    // Raw data for debugging
    std::span<const uint8_t> m_rawData;
    
    QueryParams m_queryParams;
    bool m_queryParsed = false;
    void parseQueryString();
};

// ============================================================================
// Request Builder (for creating outgoing requests)
// ============================================================================

class RequestBuilder {
public:
    RequestBuilder& setMethod(Method method);
    RequestBuilder& setPath(std::string_view path);
    RequestBuilder& setVersion(Version version);
    RequestBuilder& setHeader(std::string_view name, std::string_view value);
    RequestBuilder& setBody(std::span<const uint8_t> data);
    RequestBuilder& setBody(std::string_view data);
    RequestBuilder& setContentType(std::string_view type);
    
    Request build();
    
private:
    Request m_request;
};

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_REQUEST_H
