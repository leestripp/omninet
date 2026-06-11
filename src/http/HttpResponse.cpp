// OmniNet - HTTP Response Implementation
#include "HttpResponse.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <cstring>

#if OMNINET_HAVE_ZLIB
#include <zlib.h>
#endif

namespace OmniNet::HTTP {

// ============================================================================
// Status Codes
// ============================================================================

const char* statusReason(StatusCode code) {
    return statusReason(static_cast<int>(code));
}

const char* statusReason(int code) {
    switch (code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 421: return "Misdirected Request";
        case 422: return "Unprocessable Entity";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";
        case 507: return "Insufficient Storage";
        case 508: return "Loop Detected";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";
        default: return "Unknown";
    }
}

// ============================================================================
// Response
// ============================================================================

Response::Response() = default;

void Response::setStatus(StatusCode code) {
    m_status = code;
}

void Response::setStatus(int code) {
    m_status = static_cast<StatusCode>(code);
}

StatusCode Response::status() const {
    return m_status;
}

int Response::statusInt() const {
    return static_cast<int>(m_status);
}

std::string_view Response::reason() const {
    return statusReason(m_status);
}

void Response::setHeader(std::string_view name, std::string_view value) {
    m_headers[std::string(name)] = std::string(value);
}

void Response::setHeader(std::string_view name, std::string&& value) {
    m_headers[std::string(name)] = std::move(value);
}

void Response::removeHeader(std::string_view name) {
    m_headers.erase(std::string(name));
}

std::optional<std::string_view> Response::header(std::string_view name) const {
    auto it = m_headers.find(std::string(name));
    if (it != m_headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Response::hasHeader(std::string_view name) const {
    return m_headers.find(std::string(name)) != m_headers.end();
}

void Response::setContentType(std::string_view type) {
    setHeader("Content-Type", type);
}

void Response::setContentLength(size_t len) {
    setHeader("Content-Length", std::to_string(len));
}

void Response::setContentEncoding(const std::string& encoding) {
    setHeader("Content-Encoding", encoding);
}

void Response::setLocation(std::string_view uri) {
    setHeader("Location", uri);
}

void Response::setCacheControl(std::string_view value) {
    setHeader("Cache-Control", value);
}

void Response::setLastModified(std::string_view time) {
    setHeader("Last-Modified", time);
}

void Response::setETag(std::string_view etag) {
    setHeader("ETag", etag);
}

void Response::setKeepAlive(std::chrono::seconds timeout) {
    setHeader(std::string("Keep-Alive"), std::string("timeout=") + std::to_string(timeout.count()));
    setHeader(std::string("Connection"), std::string("keep-alive"));
}

void Response::setConnectionClose() {
    setHeader(std::string("Connection"), std::string("close"));
}

void Response::setTransferEncoding(const std::string& encoding) {
    setHeader("Transfer-Encoding", encoding);
}

void Response::setAccessControlAllowOrigin(std::string_view origin) {
    setHeader("Access-Control-Allow-Origin", origin);
}

void Response::setAccessControlAllowMethods(std::string_view methods) {
    setHeader("Access-Control-Allow-Methods", methods);
}

void Response::setAccessControlAllowHeaders(std::string_view headers) {
    setHeader("Access-Control-Allow-Headers", headers);
}

void Response::setAccessControlMaxAge(std::chrono::seconds maxAge) {
    setHeader("Access-Control-Max-Age", std::to_string(maxAge.count()));
}

void Response::setBody(std::string_view data) {
    m_body.assign(data.begin(), data.end());
    if (!hasHeader("Content-Length")) {
        setContentLength(m_body.size());
    }
}

void Response::setBody(std::span<const uint8_t> data) {
    m_body.assign(data.begin(), data.end());
    if (!hasHeader("Content-Length")) {
        setContentLength(m_body.size());
    }
}

void Response::setBody(std::vector<uint8_t>&& data) {
    m_body = std::move(data);
    if (!hasHeader("Content-Length")) {
        setContentLength(m_body.size());
    }
}

std::span<const uint8_t> Response::body() const {
    return m_body;
}

size_t Response::bodySize() const {
    return m_body.size();
}

bool Response::hasBody() const {
    return !m_body.empty() || m_filePath.has_value();
}

void Response::sendFile(const std::filesystem::path& path) {
    m_filePath = path;
}

bool Response::hasFile() const {
    return m_filePath.has_value();
}

const std::filesystem::path& Response::filePath() const {
    return *m_filePath;
}

// ============================================================================
// Gzip Compression Helper
// ============================================================================

#if OMNINET_HAVE_ZLIB

static std::vector<uint8_t> gzipCompress(const uint8_t* data, size_t size) {
    std::vector<uint8_t> compressed;
    
    // 16-byte header + compressed data + 8-byte trailer
    compressed.reserve(size + 32);
    
    // gzip header
    compressed.insert(compressed.end(), {
        0x1f, 0x8b,                    // Magic number
        0x08,                        // Compression method (deflate)
        0x00,                        // Flags
        0x00, 0x00, 0x00, 0x00,      // Modification time
        0x00,                        // Extra flags
        0xff                         // Operating system (Unix)
    });
    
    // Initialize deflate
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // Window size = 15 (same as gzip), +16 for gzip format
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }
    
    strm.next_in = const_cast<uint8_t*>(data);
    strm.avail_in = static_cast<uInt>(size);
    
    // Process in chunks
    uint8_t outBuffer[8192];
    int ret;
    
    do {
        strm.next_out = outBuffer;
        strm.avail_out = sizeof(outBuffer);
        
        ret = deflate(&strm, Z_FINISH);
        
        size_t produced = sizeof(outBuffer) - strm.avail_out;
        compressed.insert(compressed.end(), outBuffer, outBuffer + produced);
        
    } while (ret != Z_STREAM_END);
    
    deflateEnd(&strm);
    
    // Append CRC32 and size (little-endian)
    uint32_t crc = crc32(0L, data, static_cast<uInt>(size));
    uint32_t origSize = static_cast<uint32_t>(size);
    
    compressed.insert(compressed.end(), 
        reinterpret_cast<uint8_t*>(&crc),
        reinterpret_cast<uint8_t*>(&crc) + 4);
    compressed.insert(compressed.end(),
        reinterpret_cast<uint8_t*>(&origSize),
        reinterpret_cast<uint8_t*>(&origSize) + 4);
    
    return compressed;
}

#endif // OMNINET_HAVE_ZLIB

// ============================================================================
// Response - Compression
// ============================================================================

bool Response::compressBody(const std::string& acceptEncoding) {
#if !OMNINET_HAVE_ZLIB
    (void)acceptEncoding;
    return false;
#else
    // Check if client accepts gzip
    if (acceptEncoding.find("gzip") == std::string::npos) {
        return false;
    }
    
    // Only compress non-empty bodies that aren't already compressed
    if (bodySize() == 0 || m_compressed) {
        return false;
    }
    
    // Compress the body
    auto compressed = gzipCompress(m_body.data(), m_body.size());
    
    if (compressed.empty()) {
        return false;
    }
    
    m_compressedBody = std::move(compressed);
    m_compressed = true;
    
    // Update headers
    setHeader("Content-Encoding", std::string("gzip"));
    setHeader("Content-Length", std::to_string(m_compressedBody.size()));
    
    return true;
#endif
}

std::string Response::buildStatusLine() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << static_cast<int>(m_status) << " " << reason() << "\r\n";
    return oss.str();
}

std::string Response::buildHeaders() const {
    std::ostringstream oss;
    for (const auto& [name, value] : m_headers) {
        oss << name << ": " << value << "\r\n";
    }
    return oss.str();
}

size_t Response::calculateContentLength() const {
    if (hasFile()) {
        // Would need to stat the file
        return 0;
    }
    return m_body.size();
}

std::vector<uint8_t> Response::build() const {
    std::vector<uint8_t> result;

    // Status line
    auto statusLine = buildStatusLine();
    result.insert(result.end(), statusLine.begin(), statusLine.end());

    // Build headers (with auto Content-Length if not set)
    std::ostringstream hdrOss;
    // If no Content-Length is set, compute from body size so HTTP/1.1
    // keep-alive clients can detect end-of-response (v0.6.0 fix: previously
    // empty-body responses had no Content-Length, causing curl to hang).
    // We do this in a copy because m_headers is const here.
    bool needContentLength = !hasHeader("Content-Length") &&
                            !hasHeader("Transfer-Encoding") &&
                            !hasFile();
    size_t autoContentLength = 0;
    if (needContentLength) {
        autoContentLength = calculateContentLength();
    }
    for (const auto& [name, value] : m_headers) {
        hdrOss << name << ": " << value << "\r\n";
    }
    if (needContentLength) {
        hdrOss << "Content-Length: " << autoContentLength << "\r\n";
    }
    auto headers = hdrOss.str();
    result.insert(result.end(), headers.begin(), headers.end());

    // End of headers
    result.insert(result.end(), {'\r', '\n'});

    return result;
}

std::vector<uint8_t> Response::buildFull() const {
    auto response = build();
    
    // Add body
    if (hasFile()) {
        // Read and append file content
        std::ifstream file(filePath(), std::ios::binary);
        if (file) {
            // Get file size
            file.seekg(0, std::ios::end);
            auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            
            // Reserve space and read
            response.reserve(response.size() + static_cast<size_t>(fileSize));
            uint8_t buffer[8192];
            while (file.good()) {
                file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
                response.insert(response.end(), buffer, buffer + file.gcount());
            }
        }
    } else {
        response.insert(response.end(), m_body.begin(), m_body.end());
    }
    
    return response;
}

// ============================================================================
// Static Factory Methods
// ============================================================================

Response Response::ok() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    return resp;
}

Response Response::created(std::string_view location) {
    Response resp;
    resp.setStatus(StatusCode::CREATED);
    if (!location.empty()) {
        resp.setLocation(location);
    }
    return resp;
}

Response Response::noContent() {
    Response resp;
    resp.setStatus(StatusCode::NO_CONTENT);
    return resp;
}

Response Response::movedPermanently(std::string_view location) {
    Response resp;
    resp.setStatus(StatusCode::MOVED_PERMANENTLY);
    resp.setLocation(location);
    return resp;
}

Response Response::found(std::string_view location) {
    Response resp;
    resp.setStatus(StatusCode::FOUND);
    resp.setLocation(location);
    return resp;
}

Response Response::seeOther(std::string_view location) {
    Response resp;
    resp.setStatus(StatusCode::SEE_OTHER);
    resp.setLocation(location);
    return resp;
}

Response Response::temporaryRedirect(std::string_view location) {
    Response resp;
    resp.setStatus(StatusCode::TEMPORARY_REDIRECT);
    resp.setLocation(location);
    return resp;
}

Response Response::badRequest(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::BAD_REQUEST);
    if (!message.empty()) {
        resp.setBody(message);
        resp.setContentType("text/plain");
    }
    return resp;
}

Response Response::unauthorized(std::string_view challenge) {
    Response resp;
    resp.setStatus(StatusCode::UNAUTHORIZED);
    if (!challenge.empty()) {
        resp.setHeader("WWW-Authenticate", challenge);
    }
    return resp;
}

Response Response::forbidden(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::FORBIDDEN);
    if (!message.empty()) {
        resp.setBody(message);
        resp.setContentType("text/plain");
    }
    return resp;
}

Response Response::notFound(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::NOT_FOUND);
    if (!message.empty()) {
        resp.setBody(message);
        resp.setContentType("text/plain");
    }
    return resp;
}

Response Response::methodNotAllowed(std::string_view allowed) {
    Response resp;
    resp.setStatus(StatusCode::METHOD_NOT_ALLOWED);
    if (!allowed.empty()) {
        resp.setHeader("Allow", allowed);
    }
    return resp;
}

Response Response::payloadTooLarge(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::PAYLOAD_TOO_LARGE);
    if (!message.empty()) {
        resp.setBody(message);
    }
    return resp;
}

Response Response::unsupportedMediaType(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::UNSUPPORTED_MEDIA_TYPE);
    if (!message.empty()) {
        resp.setBody(message);
    }
    return resp;
}

Response Response::tooManyRequests(std::string_view retryAfter) {
    Response resp;
    resp.setStatus(StatusCode::TOO_MANY_REQUESTS);
    if (!retryAfter.empty()) {
        resp.setHeader("Retry-After", retryAfter);
    }
    return resp;
}

Response Response::internalError(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::INTERNAL_SERVER_ERROR);
    if (!message.empty()) {
        resp.setBody(message);
        resp.setContentType("text/plain");
    }
    return resp;
}

Response Response::notImplemented(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::NOT_IMPLEMENTED);
    if (!message.empty()) {
        resp.setBody(message);
    }
    return resp;
}

Response Response::serviceUnavailable(std::string_view message) {
    Response resp;
    resp.setStatus(StatusCode::SERVICE_UNAVAILABLE);
    if (!message.empty()) {
        resp.setBody(message);
    }
    return resp;
}

// ============================================================================
// Response Builder
// ============================================================================

ResponseBuilder& ResponseBuilder::setStatus(StatusCode code) {
    m_response.setStatus(code);
    return *this;
}

ResponseBuilder& ResponseBuilder::setStatus(int code) {
    m_response.setStatus(code);
    return *this;
}

ResponseBuilder& ResponseBuilder::setHeader(std::string_view name, std::string_view value) {
    m_response.setHeader(name, value);
    return *this;
}

ResponseBuilder& ResponseBuilder::setContentType(std::string_view type) {
    m_response.setContentType(type);
    return *this;
}

ResponseBuilder& ResponseBuilder::setBody(std::string_view data) {
    m_response.setBody(data);
    return *this;
}

ResponseBuilder& ResponseBuilder::setBody(std::span<const uint8_t> data) {
    m_response.setBody(data);
    return *this;
}

ResponseBuilder& ResponseBuilder::sendFile(const std::filesystem::path& path) {
    m_response.sendFile(path);
    return *this;
}

ResponseBuilder& ResponseBuilder::setKeepAlive() {
    m_response.setKeepAlive(std::chrono::seconds(30));
    return *this;
}

ResponseBuilder& ResponseBuilder::setConnectionClose() {
    m_response.setConnectionClose();
    return *this;
}

Response ResponseBuilder::build() {
    return std::move(m_response);
}

// ============================================================================
// Response Utilities
// ============================================================================

namespace ResponseUtil {

Response json(std::span<const uint8_t> jsonData) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(jsonData);
    return resp;
}

Response json(std::string_view jsonString) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(jsonString);
    return resp;
}

Response html(std::string_view html) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/html; charset=utf-8");
    resp.setBody(html);
    return resp;
}

Response text(std::string_view text) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody(text);
    return resp;
}

Response redirect(std::string_view location, StatusCode status) {
    Response resp;
    resp.setStatus(status);
    resp.setLocation(location);
    return resp;
}

Response error(StatusCode code, std::string_view message) {
    std::ostringstream body;
    body << "<html><head><title>" << static_cast<int>(code) << " " 
         << statusReason(code) << "</title></head><body><h1>"
         << statusReason(code) << "</h1>";
    if (!message.empty()) {
        body << "<p>" << message << "</p>";
    }
    body << "</body></html>";
    
    Response resp;
    resp.setStatus(code);
    resp.setContentType("text/html; charset=utf-8");
    resp.setBody(body.str());
    return resp;
}

Response corsPreflight(
    std::string_view origin,
    std::string_view methods,
    std::string_view headers,
    std::chrono::seconds maxAge
) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setAccessControlAllowOrigin(origin);
    resp.setAccessControlAllowMethods(methods);
    resp.setAccessControlAllowHeaders(headers);
    resp.setAccessControlMaxAge(maxAge);
    return resp;
}

std::string computeETag(std::span<const uint8_t> content) {
    // Simple ETag: size-mtime (could use MD5 for full content)
    std::ostringstream oss;
    oss << "\"" << content.size() << "\"";
    return oss.str();
}

bool isNotModified(std::string_view etag, std::string_view ifNoneMatch,
                  std::string_view ifModifiedSince) {
    // If-None-Match check
    if (!ifNoneMatch.empty()) {
        // "*" means match any - treat as not modified
        if (ifNoneMatch == "*") {
            return true;
        }
        // Check if our ETag matches any in the list
        // If-None-Match can be a comma-separated list
        size_t start = 0;
        while (start < ifNoneMatch.size()) {
            size_t comma = ifNoneMatch.find(',', start);
            size_t end = (comma == std::string_view::npos) ? ifNoneMatch.size() : comma;
            
            // Trim whitespace
            while (start < end && std::isspace(static_cast<unsigned char>(ifNoneMatch[start]))) start++;
            while (end > start && std::isspace(static_cast<unsigned char>(ifNoneMatch[end-1]))) end--;
            
            if (etag == ifNoneMatch.substr(start, end - start)) {
                return true;  // ETag matches, return 304
            }
            
            if (comma == std::string_view::npos) break;
            start = comma + 1;
        }
    }
    
    // If-Modified-Since check (only if If-None-Match didn't match)
    if (!ifModifiedSince.empty()) {
        // Simple implementation: compare as string
        // A full implementation would parse HTTP-date and compare with Last-Modified
        // For now, we return false to indicate modification detected
        // (which means the client should receive the full response)
        (void)ifModifiedSince;
        // Return false to indicate we don't have a cached version to compare
        // In a full implementation, you'd compare with actual Last-Modified header
        return false;
    }
    
    return false;
}

} // namespace ResponseUtil

} // namespace OmniNet::HTTP
