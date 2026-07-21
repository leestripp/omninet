// OmniNet - Utility Functions Implementation
#include "Utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <streambuf>

// OpenSSL includes for HMAC-SHA256 (v0.6.0: hmacSha256 now delegates
// to Crypto::hmacSha256 which uses the EVP_MAC API, so HMAC_CTX
// is no longer needed here)
#include <openssl/evp.h>
#include "crypto/Crypto.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include "crypto/Crypto.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace OmniNet::Utils {

// ============================================================================
// String Utilities
// ============================================================================

std::vector<std::string> split(std::string_view str, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    
    while (start < str.size()) {
        auto pos = str.find(delim, start);
        if (pos == std::string_view::npos) {
            pos = str.size();
        }
        result.emplace_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    
    return result;
}

std::vector<std::string> splitAny(std::string_view str, std::string_view delims) {
    std::vector<std::string> result;
    size_t start = 0;
    
    while (start < str.size()) {
        auto pos = str.find_first_of(delims, start);
        if (pos == std::string_view::npos) {
            pos = str.size();
        }
        result.emplace_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    
    return result;
}

std::string trim(std::string_view str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    
    return std::string(str.substr(start, end - start));
}

std::string trimLeft(std::string_view str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    return std::string(str.substr(start));
}

std::string trimRight(std::string_view str) {
    size_t end = str.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return std::string(str.substr(0, end));
}

std::string join(const std::vector<std::string>& parts, const std::string& delim) {
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    oss << parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        oss << delim << parts[i];
    }
    return oss.str();
}

std::string toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string toUpper(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

bool startsWith(std::string_view str, std::string_view prefix) {
    if (prefix.size() > str.size()) return false;
    return str.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view str, std::string_view suffix) {
    if (suffix.size() > str.size()) return false;
    return str.substr(str.size() - suffix.size()) == suffix;
}

std::string replaceAll(std::string_view str, std::string_view from, std::string_view to) {
    std::string result;
    size_t pos = 0;
    
    while (pos < str.size()) {
        auto found = str.find(from, pos);
        if (found == std::string_view::npos) {
            result += str.substr(pos);
            break;
        }
        result += str.substr(pos, found - pos);
        result += to;
        pos = found + from.size();
    }
    
    return result;
}

// ============================================================================
// URL Encoding
// ============================================================================

bool isUnreserved(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

std::string urlEncode(std::string_view str) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    
    for (unsigned char c : str) {
        if (isUnreserved(c)) {
            oss << c;
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    
    return oss.str();
}

std::string urlEncodePath(std::string_view str) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    
    for (unsigned char c : str) {
        if (isUnreserved(c) || c == '/' || c == ':' || c == '@') {
            oss << c;
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    
    return oss.str();
}

std::string urlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '%' && i + 2 < str.size()) {
            unsigned int value = 0;
            std::sscanf(str.substr(i + 1, 2).data(), "%02x", &value);
            result += static_cast<char>(value);
            i += 3;
        } else if (str[i] == '+') {
            result += ' ';
            ++i;
        } else {
            result += str[i];
            ++i;
        }
    }
    
    return result;
}

// ============================================================================
// Base64
// ============================================================================

static const char* BASE64_CHARS = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(std::span<const uint8_t> data) {
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    
    size_t i = 0;
    while (i < data.size()) {
        uint32_t n = (data[i] << 16);
        if (i + 1 < data.size()) n |= (data[i + 1] << 8);
        if (i + 2 < data.size()) n |= data[i + 2];
        
        result += BASE64_CHARS[(n >> 18) & 0x3f];
        result += BASE64_CHARS[(n >> 12) & 0x3f];
        result += (i + 1 < data.size()) ? BASE64_CHARS[(n >> 6) & 0x3f] : '=';
        result += (i + 2 < data.size()) ? BASE64_CHARS[n & 0x3f] : '=';
        
        i += 3;
    }
    
    return result;
}

std::string base64Encode(std::string_view str) {
    return base64Encode(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.size()
    ));
}

static int base64CharIndex(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::optional<std::vector<uint8_t>> base64Decode(std::string_view encoded) {
    // Remove whitespace
    std::string clean;
    clean.reserve(encoded.size());
    for (char c : encoded) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            clean += c;
        }
    }
    
    if (clean.size() % 4 != 0) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> result;
    result.reserve(clean.size() * 3 / 4);
    
    size_t i = 0;
    while (i < clean.size()) {
        int idx0 = base64CharIndex(clean[i]);
        int idx1 = base64CharIndex(clean[i + 1]);
        int idx2 = clean[i + 2] == '=' ? 0 : base64CharIndex(clean[i + 2]);
        int idx3 = clean[i + 3] == '=' ? 0 : base64CharIndex(clean[i + 3]);
        
        if (idx0 < 0 || idx1 < 0 || idx2 < 0 || idx3 < 0) {
            return std::nullopt;
        }
        
        uint32_t n = (idx0 << 18) | (idx1 << 12) | (idx2 << 6) | idx3;
        
        result.push_back(static_cast<uint8_t>((n >> 16) & 0xff));
        if (clean[i + 2] != '=') result.push_back(static_cast<uint8_t>((n >> 8) & 0xff));
        if (clean[i + 3] != '=') result.push_back(static_cast<uint8_t>(n & 0xff));
        
        i += 4;
    }
    
    return result;
}

std::optional<std::string> base64DecodeToString(std::string_view encoded) {
    auto result = base64Decode(encoded);
    if (!result) return std::nullopt;
    return std::string(result->begin(), result->end());
}

// ============================================================================
// Hex Conversion
// ============================================================================

std::string bytesToHex(std::span<const uint8_t> bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (uint8_t b : bytes) {
        oss << std::setw(2) << (int)b;
    }
    
    return oss.str();
}

std::string bytesToHexUpper(std::span<const uint8_t> bytes) {
    std::string result = bytesToHex(bytes);
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::optional<std::vector<uint8_t>> hexToBytes(std::string_view hex) {
    std::string clean;
    clean.reserve(hex.size());
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            clean += c;
        }
    }
    
    if (clean.size() % 2 != 0) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> result;
    result.reserve(clean.size() / 2);
    
    for (size_t i = 0; i < clean.size(); i += 2) {
        unsigned int value = 0;
        if (std::sscanf(clean.substr(i, 2).data(), "%02x", &value) != 1) {
            return std::nullopt;
        }
        result.push_back(static_cast<uint8_t>(value));
    }
    
    return result;
}

// ============================================================================
// Secure Random
// ============================================================================

std::vector<uint8_t> secureRandom(size_t count) {
    std::vector<uint8_t> result(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < count; ++i) {
        result[i] = static_cast<uint8_t>(dis(gen));
    }
    
    return result;
}

std::string secureRandomHex(size_t count) {
    return bytesToHex(secureRandom(count));
}

std::string secureRandomAlphanumeric(size_t count) {
    static const char* chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    
    std::string result;
    result.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 63);
    
    for (size_t i = 0; i < count; ++i) {
        result += chars[dis(gen)];
    }
    
    return result;
}

// ============================================================================
// Time Utilities
// ============================================================================

std::string formatRfc2822(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time);
    
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z", &tm);
    return buffer;
}

std::string formatIso8601(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ) % 1000;
    
    std::tm tm = *std::localtime(&time);
    
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
    
    std::ostringstream oss;
    oss << buffer << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

std::optional<std::chrono::system_clock::time_point> parseRfc2822(std::string_view str) {
    std::tm tm = {};
    
    // Try various RFC 2822 formats
    const char* formats[] = {
        "%a, %d %b %Y %H:%M:%S %z",
        "%d %b %Y %H:%M:%S",
        nullptr
    };
    
    for (const char* fmt : formats) {
        if (strptime(str.data(), fmt, &tm)) {
            auto time = std::mktime(&tm);
            if (time != -1) {
                return std::chrono::system_clock::from_time_t(time);
            }
        }
    }
    
    return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point> parseIso8601(std::string_view str) {
    std::tm tm = {};
    int milliseconds = 0;
    
    // Try ISO 8601 formats
    const char* formats[] = {
        "%Y-%m-%dT%H:%M:%S.%LZ",    // 2024-01-15T10:30:45.123Z
        "%Y-%m-%dT%H:%M:%SZ",         // 2024-01-15T10:30:45Z
        "%Y-%m-%dT%H:%M:%S",          // 2024-01-15T10:30:45 (local)
        "%Y-%m-%dT%H:%M:%S.%L",       // 2024-01-15T10:30:45.123 (local, no Z)
        "%Y-%m-%d %H:%M:%S",          // 2024-01-15 10:30:45
        nullptr
    };
    
    for (const char* fmt : formats) {
        std::string s(str);
        if (strptime(s.c_str(), fmt, &tm)) {
            auto time = std::mktime(&tm);
            if (time != -1) {
                auto tp = std::chrono::system_clock::from_time_t(time);
                if (milliseconds > 0) {
                    tp += std::chrono::milliseconds(milliseconds);
                }
                return tp;
            }
        }
    }
    
    // Try parsing milliseconds manually for formats ending with .123Z
    if (str.size() >= 24 && str[str.size() - 1] == 'Z') {
        std::string s(str);
        // Find the . before milliseconds
        auto dotPos = s.find('.');
        if (dotPos != std::string::npos) {
            // Zero out the milliseconds part temporarily
            auto msStart = dotPos + 1;
            auto msEnd = s.size() - 1;  // Before Z
            if (msEnd > msStart && msEnd - msStart <= 3) {
                std::string msStr = s.substr(msStart, msEnd - msStart);
                milliseconds = std::stoi(msStr);
                // Temporarily replace with format we can parse
                s.erase(dotPos);
                s += 'Z';
            }
        }
        
        if (strptime(s.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm)) {
            auto time = std::mktime(&tm);
            if (time != -1) {
                auto tp = std::chrono::system_clock::from_time_t(time);
                if (milliseconds > 0) {
                    tp += std::chrono::milliseconds(milliseconds);
                }
                return tp;
            }
        }
    }
    
    return std::nullopt;
}

int64_t currentUnixTime() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// ============================================================================
// Path Utilities
// ============================================================================

std::string normalizePath(std::string_view path) {
    std::vector<std::string> parts = split(path, '/');
    std::vector<std::string> result;
    
    for (const auto& part : parts) {
        if (part.empty() || part == ".") {
            continue;
        } else if (part == "..") {
            if (!result.empty()) {
                result.pop_back();
            }
        } else {
            result.push_back(part);
        }
    }
    
    std::string out = join(result, "/");
    // Preserve whether path was relative or absolute
    if (path.starts_with("/")) {
        out = "/" + out;
    }
    
    return out;
}

std::string_view directoryOf(std::string_view path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string_view::npos) {
        return ".";
    }
    return path.substr(0, pos);
}

std::string_view filenameOf(std::string_view path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string_view::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string_view extensionOf(std::string_view path) {
    auto name = filenameOf(path);
    auto pos = name.find_last_of('.');
    if (pos == std::string_view::npos || pos == 0) {
        return "";
    }
    return name.substr(pos);
}

bool isAbsolute(std::string_view path) {
    return !path.empty() && path[0] == '/';
}

// ============================================================================
// MIME Type
// ============================================================================

static const std::unordered_map<std::string, std::string> MIME_TYPES = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".txt", "text/plain"},
    {".csv", "text/csv"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".webp", "image/webp"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".gz", "application/gzip"},
    {".tar", "application/x-tar"},
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},
};

std::string mimeType(std::string_view path) {
    auto ext = extensionOf(path);
    auto it = MIME_TYPES.find(std::string(ext));
    if (it != MIME_TYPES.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string mimeTypeUpper(std::string_view path) {
    return toUpper(mimeType(path));
}

// ============================================================================
// IP Utilities
// ============================================================================

bool isValidIpv4(std::string_view addr) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, addr.data(), &(sa.sin_addr)) == 1;
}

bool isValidIpv6(std::string_view addr) {
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, addr.data(), &(sa.sin6_addr)) == 1;
}

bool isValidIp(std::string_view addr) {
    return isValidIpv4(addr) || isValidIpv6(addr);
}

bool matchesCidr(std::string_view ip, std::string_view cidr) {
    // Parse CIDR
    auto slash = cidr.find('/');
    if (slash == std::string_view::npos) {
        return ip == cidr;
    }
    
    std::string network(cidr.substr(0, slash));
    int prefixLen = std::stoi(std::string(cidr.substr(slash + 1)));
    
    // IPv4 only for now
    if (!isValidIpv4(ip) || !isValidIpv4(network)) {
        return false;
    }
    
    uint32_t ipInt = 0, netInt = 0;
    inet_pton(AF_INET, std::string(ip).c_str(), &ipInt);
    inet_pton(AF_INET, std::string(network).c_str(), &netInt);
    
    uint32_t mask = prefixLen == 0 ? 0 : ~((1u << (32 - prefixLen)) - 1);
    
    return (ntohl(ipInt) & mask) == (ntohl(netInt) & mask);
}

// ============================================================================
// Misc Utilities
// ============================================================================

std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    size_t unitIdx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unitIdx < 4) {
        size /= 1024;
        ++unitIdx;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIdx];
    return oss.str();
}

std::string formatDuration(std::chrono::seconds duration) {
    auto secs = duration.count();
    
    if (secs < 60) {
        return std::to_string(secs) + "s";
    } else if (secs < 3600) {
        return std::to_string(secs / 60) + "m";
    } else if (secs < 86400) {
        return std::to_string(secs / 3600) + "h";
    } else {
        return std::to_string(secs / 86400) + "d";
    }
}

std::string escapeHtml(std::string_view str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '&': oss << "&amp;"; break;
            case '<': oss << "&lt;"; break;
            case '>': oss << "&gt;"; break;
            case '"': oss << "&quot;"; break;
            case '\'': oss << "&#39;"; break;
            default: oss << c;
        }
    }
    return oss.str();
}

std::string unescapeHtml(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '&') {
            if (str.substr(i, 5) == "&amp;") { result += '&'; i += 5; }
            else if (str.substr(i, 4) == "&lt;") { result += '<'; i += 4; }
            else if (str.substr(i, 4) == "&gt;") { result += '>'; i += 4; }
            else if (str.substr(i, 6) == "&quot;") { result += '"'; i += 6; }
            else if (str.substr(i, 5) == "&#39;") { result += '\''; i += 5; }
            else { result += str[i]; ++i; }
        } else {
            result += str[i];
            ++i;
        }
    }
    
    return result;
}

std::optional<std::vector<uint8_t>> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> result(size);
    file.read(reinterpret_cast<char*>(result.data()), size);
    
    return result;
}

bool writeFile(const std::string& path, std::span<const uint8_t> data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

// ============================================================================
// HMAC-SHA256 (v0.6.0 fixes bugs #8 + #12)
// Previously this was a duplicate of Crypto::hmacSha256 using the
// deprecated HMAC_CTX_* API. Now delegates to Crypto (which uses the
// modern EVP_MAC API) to avoid the duplication and the OpenSSL 3.0
// deprecation warnings.
std::string hmacSha256(std::string_view key, std::string_view message) {
    auto raw = Crypto::hmacSha256(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()),
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(message.data()), message.size())
    );
    // Convert to hex string
    static const char hex[] = "0123456789abcdef";
    std::string hexStr;
    hexStr.reserve(raw.size() * 2);
    for (auto b : raw) {
        hexStr += hex[(b >> 4) & 0xf];
        hexStr += hex[b & 0xf];
    }
    return hexStr;
}

} // namespace OmniNet::Utils
