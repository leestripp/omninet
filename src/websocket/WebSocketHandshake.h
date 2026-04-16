// OmniNet - WebSocket Handshake
#ifndef OMNINET_WS_HANDSHAKE_H
#define OMNINET_WS_HANDSHAKE_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace OmniNet::WebSocket {

// ============================================================================
// WebSocket Handshake
// ============================================================================

class Handshake {
public:
    // --- Server-side ---
    
    // Process client upgrade request
    // Returns Sec-WebSocket-Accept key on success
    static std::optional<std::string> processClientRequest(
        std::string_view request,
        std::unordered_map<std::string, std::string>& headers,
        std::string& error
    );
    
    // Generate server response
    static std::string generateServerResponse(
        const std::string& webSocketKey,
        std::string_view protocol = {},
        std::string_view extensions = {}
    );
    
    // --- Client-side ---
    
    // Generate client request
    static std::string generateClientRequest(
        std::string_view host,
        uint16_t port,
        std::string_view path,
        std::string_view key,
        std::string_view protocol = {},
        std::string_view extensions = {},
        std::string_view origin = {}
    );
    
    // Validate server response
    static bool validateServerResponse(
        std::string_view response,
        const std::string& clientKey,
        std::string& error
    );
    
    // --- Key Generation ---
    
    // Generate random 16-byte key for client
    static std::string generateClientKey();
    
    // Compute accept key from client key
    static std::string computeAcceptKey(const std::string& clientKey);
    
    // --- URL Utilities ---
    
    // Parse WebSocket URL
    struct Url {
        bool secure = false;
        std::string host;
        uint16_t port = 0;
        std::string path;
    };
    
    static std::optional<Url> parseUrl(std::string_view url);
    
    // Generate WebSocket URL
    static std::string makeUrl(
        bool secure,
        const std::string& host,
        uint16_t port,
        std::string_view path
    );
    
    // --- Constants ---
    
    static constexpr const char* MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    static constexpr const char* PROTOCOL_VERSION = "13";

private:
    static std::string base64Encode(const uint8_t* data, size_t len);
};

// ============================================================================
// WebSocket Headers
// ============================================================================

struct Headers {
    std::string upgrade;
    std::string connection;
    std::string webSocketVersion;
    std::string webSocketKey;
    std::string webSocketProtocol;
    std::string webSocketExtensions;
    std::string origin;
    std::string host;
    std::string userAgent;
};

std::optional<Headers> parseHeaders(
    const std::unordered_map<std::string, std::string>& headers
);

} // namespace OmniNet::WebSocket

#endif // OMNINET_WS_HANDSHAKE_H
