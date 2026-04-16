// OmniNet - WebSocket Handshake Implementation
#include "WebSocketHandshake.h"

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include <array>
#include <random>

namespace OmniNet::WebSocket {

// ============================================================================
// Handshake
// ============================================================================

std::optional<std::string> Handshake::processClientRequest(
    std::string_view request,
    std::unordered_map<std::string, std::string>& headers,
    std::string& error
) {
    // Parse headers
    size_t pos = 0;
    std::string currentHeader;
    std::string currentValue;
    bool parsingHeader = true;
    
    while (pos < request.size()) {
        char c = request[pos];
        
        if (c == '\n') {
            if (currentHeader.empty() && currentValue.empty()) {
                // Empty line = end of headers
                break;
            }
            
            headers[Utils::toLower(Utils::trim(currentHeader))] = Utils::trim(currentValue);
            currentHeader.clear();
            currentValue.clear();
            parsingHeader = true;
        } else if (c == ':' && parsingHeader) {
            parsingHeader = false;
        } else if (c == '\r') {
            // Skip
        } else {
            if (parsingHeader) {
                currentHeader += c;
            } else {
                currentValue += c;
            }
        }
        
        ++pos;
    }
    
    // Validate upgrade
    auto upgrade = headers.find("upgrade");
    if (upgrade == headers.end() || Utils::toLower(upgrade->second) != "websocket") {
        error = "Missing or invalid Upgrade header";
        return std::nullopt;
    }
    
    // Validate connection
    auto connection = headers.find("connection");
    if (connection == headers.end() || Utils::toLower(connection->second) != "upgrade") {
        error = "Missing or invalid Connection header";
        return std::nullopt;
    }
    
    // Get key
    auto key = headers.find("sec-websocket-key");
    if (key == headers.end()) {
        error = "Missing Sec-WebSocket-Key header";
        return std::nullopt;
    }
    
    // Validate version
    auto version = headers.find("sec-websocket-version");
    if (version == headers.end() || version->second != "13") {
        error = "Unsupported WebSocket version";
        return std::nullopt;
    }
    
    // Compute accept key
    return computeAcceptKey(key->second);
}

std::string Handshake::generateServerResponse(
    const std::string& webSocketKey,
    std::string_view protocol,
    std::string_view extensions
) {
    auto acceptKey = computeAcceptKey(webSocketKey);
    
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + acceptKey + "\r\n";
    
    if (!protocol.empty()) {
        response += "Sec-WebSocket-Protocol: " + std::string(protocol) + "\r\n";
    }
    
    if (!extensions.empty()) {
        response += "Sec-WebSocket-Extensions: " + std::string(extensions) + "\r\n";
    }
    
    response += "\r\n";
    
    return response;
}

std::string Handshake::generateClientRequest(
    std::string_view host,
    uint16_t port,
    std::string_view path,
    std::string_view key,
    std::string_view protocol,
    std::string_view extensions,
    std::string_view origin
) {
    std::string request = "GET " + std::string(path) + " HTTP/1.1\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Host: " + std::string(host) + ":" + std::to_string(port) + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "Sec-WebSocket-Key: " + std::string(key) + "\r\n";
    
    if (!protocol.empty()) {
        request += "Sec-WebSocket-Protocol: " + std::string(protocol) + "\r\n";
    }
    
    if (!extensions.empty()) {
        request += "Sec-WebSocket-Extensions: " + std::string(extensions) + "\r\n";
    }
    
    if (!origin.empty()) {
        request += "Origin: " + std::string(origin) + "\r\n";
    }
    
    request += "\r\n";
    
    return request;
}

bool Handshake::validateServerResponse(
    std::string_view response,
    const std::string& clientKey,
    std::string& error
) {
    // Parse status line
    auto lines = Utils::split(std::string(response), '\n');
    if (lines.empty()) {
        error = "Empty response";
        return false;
    }
    
    // Check status code
    auto statusLine = Utils::trim(lines[0]);
    if (statusLine.find("101") == std::string::npos) {
        error = "Expected 101 Switching Protocols, got: " + statusLine;
        return false;
    }
    
    // Find Sec-WebSocket-Accept header
    auto acceptKey = computeAcceptKey(clientKey);
    bool foundAccept = false;
    
    for (size_t i = 1; i < lines.size(); ++i) {
        auto line = Utils::trim(lines[i]);
        if (line.empty()) break;
        
        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            auto name = Utils::toLower(Utils::trim(line.substr(0, colonPos)));
            auto value = Utils::trim(line.substr(colonPos + 1));
            
            if (name == "sec-websocket-accept") {
                if (value != acceptKey) {
                    error = "Invalid Sec-WebSocket-Accept key";
                    return false;
                }
                foundAccept = true;
                break;
            }
        }
    }
    
    if (!foundAccept) {
        error = "Missing Sec-WebSocket-Accept header";
        return false;
    }
    
    return true;
}

std::string Handshake::generateClientKey() {
    // Generate 16 random bytes, base64 encoded
    std::array<uint8_t, 16> bytes;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& b : bytes) {
        b = static_cast<uint8_t>(dis(gen));
    }
    
    return base64Encode(bytes.data(), 16);
}

std::string Handshake::computeAcceptKey(const std::string& clientKey) {
    // SHA-1(clientKey + MAGIC_STRING), then base64
    std::string data = clientKey + MAGIC_STRING;
    
    // Compute SHA-1
    std::array<uint8_t, 20> hash;
    SHA1(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hash.data());
    
    return base64Encode(hash.data(), 20);
}

std::optional<Handshake::Url> Handshake::parseUrl(std::string_view url) {
    Url result;
    
    // Parse scheme
    if (url.starts_with("ws://")) {
        result.secure = false;
        url = url.substr(5);
        result.port = 80;
    } else if (url.starts_with("wss://")) {
        result.secure = true;
        url = url.substr(6);
        result.port = 443;
    } else {
        return std::nullopt;
    }
    
    // Parse host
    auto pathStart = url.find('/');
    if (pathStart == std::string_view::npos) {
        result.host = std::string(url);
        result.path = "/";
    } else {
        result.host = std::string(url.substr(0, pathStart));
        result.path = std::string(url.substr(pathStart));
    }
    
    // Check for port in host
    auto portPos = result.host.find(':');
    if (portPos != std::string::npos) {
        result.port = static_cast<uint16_t>(std::stoi(std::string(result.host.substr(portPos + 1))));
        result.host = std::string(result.host.substr(0, portPos));
    }
    
    return result;
}

std::string Handshake::makeUrl(
    bool secure,
    const std::string& host,
    uint16_t port,
    std::string_view path
) {
    std::string url;
    url = secure ? "wss://" : "ws://";
    url += host;
    
    if ((secure && port != 443) || (!secure && port != 80)) {
        url += ":" + std::to_string(port);
    }
    
    url += std::string(path);
    
    return url;
}

std::string Handshake::base64Encode(const uint8_t* data, size_t len) {
    static const char* chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    
    size_t i = 0;
    while (i < len) {
        uint32_t n = (data[i] << 16);
        if (i + 1 < len) n |= (data[i + 1] << 8);
        if (i + 2 < len) n |= data[i + 2];
        
        result += chars[(n >> 18) & 0x3f];
        result += chars[(n >> 12) & 0x3f];
        result += (i + 1 < len) ? chars[(n >> 6) & 0x3f] : '=';
        result += (i + 2 < len) ? chars[n & 0x3f] : '=';
        
        i += 3;
    }
    
    return result;
}

// ============================================================================
// Headers
// ============================================================================

std::optional<Headers> parseHeaders(
    const std::unordered_map<std::string, std::string>& headers
) {
    Headers result;
    
    auto it = headers.find("upgrade");
    if (it != headers.end()) result.upgrade = it->second;
    
    it = headers.find("connection");
    if (it != headers.end()) result.connection = it->second;
    
    it = headers.find("sec-websocket-version");
    if (it != headers.end()) result.webSocketVersion = it->second;
    
    it = headers.find("sec-websocket-key");
    if (it != headers.end()) result.webSocketKey = it->second;
    
    it = headers.find("sec-websocket-protocol");
    if (it != headers.end()) result.webSocketProtocol = it->second;
    
    it = headers.find("sec-websocket-extensions");
    if (it != headers.end()) result.webSocketExtensions = it->second;
    
    it = headers.find("origin");
    if (it != headers.end()) result.origin = it->second;
    
    it = headers.find("host");
    if (it != headers.end()) result.host = it->second;
    
    return result;
}

} // namespace OmniNet::WebSocket
