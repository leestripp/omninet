// OmniNet - HTTP Response
#ifndef OMNINET_HTTP_RESPONSE_H
#define OMNINET_HTTP_RESPONSE_H

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace OmniNet::HTTP {

// ============================================================================
// HTTP Status Codes
// ============================================================================

enum class StatusCode : int {
    // 1xx Informational
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,
    
    // 2xx Success
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NON_AUTHORITATIVE_INFO = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,
    
    // 3xx Redirection
    MULTIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    USE_PROXY = 305,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,
    
    // 4xx Client Error
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    PROXY_AUTH_REQUIRED = 407,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    PAYLOAD_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    TEAPOT = 418,
    MISDIRECTED_REQUEST = 421,
    UNPROCESSABLE_ENTITY = 422,
    LOCKED = 423,
    FAILED_DEPENDENCY = 424,
    UPGRADE_REQUIRED = 426,
    PRECONDITION_REQUIRED = 428,
    TOO_MANY_REQUESTS = 429,
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    RETRY_WITH = 449,
    
    // 5xx Server Error
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505,
    VARIANT_ALSO_NEGOTIATES = 506,
    INSUFFICIENT_STORAGE = 507,
    LOOP_DETECTED = 508,
    NOT_EXTENDED = 510,
    NETWORK_AUTH_REQUIRED = 511
};

const char* statusReason(StatusCode code);
const char* statusReason(int code);

// ============================================================================
// HTTP Response
// ============================================================================

class Response {
public:
    Response();
    
    // --- Status ---
    
    void setStatus(StatusCode code);
    void setStatus(int code);
    StatusCode status() const;
    int statusInt() const;
    std::string_view reason() const;
    
    // --- Headers ---

    // v0.8.3 (MonkeyByte Bug 4): the previous overload set
    //   setHeader(string_view, string&&)
    // made calls like `resp.setHeader("Cache-Control", "no-cache")`
    // ambiguous (const char[] binds equally well to string_view or
    // string&&). Removed the string&& overload; the string_view
    // overload already binds std::string rvalues via temporary
    // materialization, with one extra move-construction. For header
    // values (typically < 1 KB) the move cost is noise.
    void setHeader(std::string_view name, std::string_view value);
    void removeHeader(std::string_view name);
    
    std::optional<std::string_view> header(std::string_view name) const;
    bool hasHeader(std::string_view name) const;
    
    // Common headers
    void setContentType(std::string_view type);
    void setContentLength(size_t len);
    void setContentEncoding(const std::string& encoding);  // gzip, etc.
    void setLocation(std::string_view uri);
    void setCacheControl(std::string_view value);
    void setLastModified(std::string_view time);
    void setETag(std::string_view etag);
    
    // Connection
    void setKeepAlive(std::chrono::seconds timeout);
    void setConnectionClose();
    void setTransferEncoding(const std::string& encoding);
    
    // CORS
    void setAccessControlAllowOrigin(std::string_view origin);
    void setAccessControlAllowMethods(std::string_view methods);
    void setAccessControlAllowHeaders(std::string_view headers);
    void setAccessControlMaxAge(std::chrono::seconds maxAge);
    
    // --- Body ---
    
    void setBody(std::string_view data);
    void setBody(std::span<const uint8_t> data);
    void setBody(std::vector<uint8_t>&& data);
    
    std::span<const uint8_t> body() const;
    size_t bodySize() const;
    bool hasBody() const;
    
    // --- File Serving ---
    
    void sendFile(const std::filesystem::path& path);
    bool hasFile() const;
    const std::filesystem::path& filePath() const;
    
    // --- Compression ---
    
    // Enable gzip compression if client supports it
    bool compressBody(const std::string& acceptEncoding);
    
    // --- Serialization ---
    
    // Build raw HTTP response bytes
    std::vector<uint8_t> build() const;
    
    // Build for sending (includes file content if set)
    std::vector<uint8_t> buildFull() const;

    // v0.7.0 (prompt.txt Bug #2): protocol-switch handoff.
    // When marked, the HTTP server writes the response and then
    // hands the raw socket to a post-upgrade callback registered
    // for the same path (via HttpServer::setPostUpgradeHandler)
    // instead of looping back to read another HTTP request. Used
    // by WebSocket::Server::on() for the HTTP→WebSocket switch.
    void markProtocolSwitch();
    bool isProtocolSwitch() const;

    // --- Static Helpers ---
    
    // Common response factories
    static Response ok();
    static Response created(std::string_view location = {});
    static Response noContent();
    
    static Response movedPermanently(std::string_view location);
    static Response found(std::string_view location);
    static Response seeOther(std::string_view location);
    static Response temporaryRedirect(std::string_view location);
    
    static Response badRequest(std::string_view message = {});
    static Response unauthorized(std::string_view challenge = "Bearer");
    static Response forbidden(std::string_view message = "Access denied");
    static Response notFound(std::string_view message = "Not Found");
    static Response methodNotAllowed(std::string_view allowed = {});
    static Response payloadTooLarge(std::string_view message = {});
    static Response unsupportedMediaType(std::string_view message = {});
    static Response tooManyRequests(std::string_view retryAfter = {});
    
    static Response internalError(std::string_view message = "Internal Server Error");
    static Response notImplemented(std::string_view message = "Not Implemented");
    static Response serviceUnavailable(std::string_view message = "Service Unavailable");

private:
    StatusCode m_status = StatusCode::OK;
    std::unordered_map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
    std::optional<std::filesystem::path> m_filePath;
    
    bool m_compressed = false;
    std::vector<uint8_t> m_compressedBody;

    // v0.7.0: see markProtocolSwitch() above.
    bool m_protocolSwitch = false;

    // Build status line
    std::string buildStatusLine() const;
    
    // Build headers string
    std::string buildHeaders() const;
    
    // Calculate Content-Length header
    size_t calculateContentLength() const;
};

// ============================================================================
// Response Builder
// ============================================================================

class ResponseBuilder {
public:
    ResponseBuilder& setStatus(StatusCode code);
    ResponseBuilder& setStatus(int code);
    ResponseBuilder& setHeader(std::string_view name, std::string_view value);
    ResponseBuilder& setContentType(std::string_view type);
    ResponseBuilder& setBody(std::string_view data);
    ResponseBuilder& setBody(std::span<const uint8_t> data);
    ResponseBuilder& sendFile(const std::filesystem::path& path);
    ResponseBuilder& setKeepAlive();
    ResponseBuilder& setConnectionClose();
    
    Response build();

private:
    Response m_response;
};

// ============================================================================
// Response Utilities
// ============================================================================

namespace ResponseUtil {

// Create JSON response
Response json(std::span<const uint8_t> jsonData);
Response json(std::string_view jsonString);

// Create HTML response
Response html(std::string_view html);

// Create plain text response
Response text(std::string_view text);

// Create redirect response
Response redirect(std::string_view location, StatusCode status = StatusCode::FOUND);

// Create error response with HTML body
Response error(StatusCode code, std::string_view message);

// Create CORS preflight response
Response corsPreflight(
    std::string_view origin,
    std::string_view methods,
    std::string_view headers,
    std::chrono::seconds maxAge = std::chrono::seconds(86400)
);

// Compute ETag from content
std::string computeETag(std::span<const uint8_t> content);

// Check If-None-Match / If-Modified-Since for caching
bool isNotModified(std::string_view etag, std::string_view ifNoneMatch,
                  std::string_view ifModifiedSince);

} // namespace ResponseUtil

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_RESPONSE_H