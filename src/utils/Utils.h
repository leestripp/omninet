// OmniNet - Utility functions
#ifndef OMNINET_UTILS_H
#define OMNINET_UTILS_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OmniNet::Utils {

// ============================================================================
// String Utilities
// ============================================================================

// Split string by delimiter
std::vector<std::string> split(std::string_view str, char delim);

// Split string by any of the given delimiters
std::vector<std::string> splitAny(std::string_view str, std::string_view delims);

// Trim whitespace from both ends
std::string trim(std::string_view str);

// Trim from left only
std::string trimLeft(std::string_view str);

// Trim from right only
std::string trimRight(std::string_view str);

// Join strings with delimiter
std::string join(const std::vector<std::string>& parts, const std::string& delim);

// Convert to lowercase
std::string toLower(std::string_view str);

// Convert to uppercase
std::string toUpper(std::string_view str);

// Check if string starts with prefix
bool startsWith(std::string_view str, std::string_view prefix);

// Check if string ends with suffix
bool endsWith(std::string_view str, std::string_view suffix);

// Replace all occurrences of a substring
std::string replaceAll(std::string_view str, std::string_view from, std::string_view to);

// ============================================================================
// URL Encoding/Decoding (Percent-Encoding)
// ============================================================================

// URL-encode a string (for query parameters)
std::string urlEncode(std::string_view str);

// URL-encode a string, preserving path characters
std::string urlEncodePath(std::string_view str);

// URL-decode a string
std::string urlDecode(std::string_view str);

// Check if character is unreserved (safe to keep as-is in URL)
bool isUnreserved(char c);

// ============================================================================
// Base64 Encoding/Decoding
// ============================================================================

// Encode bytes to Base64 string
std::string base64Encode(std::span<const uint8_t> data);

// Encode string to Base64
std::string base64Encode(std::string_view str);

// Decode Base64 string to bytes
std::optional<std::vector<uint8_t>> base64Decode(std::string_view encoded);

// Decode Base64 to string
std::optional<std::string> base64DecodeToString(std::string_view encoded);

// ============================================================================
// Hex Conversion
// ============================================================================

// Convert bytes to hex string (lowercase)
std::string bytesToHex(std::span<const uint8_t> bytes);

// Convert bytes to hex string (uppercase)
std::string bytesToHexUpper(std::span<const uint8_t> bytes);

// Convert hex string to bytes
std::optional<std::vector<uint8_t>> hexToBytes(std::string_view hex);

// ============================================================================
// Secure Random
// ============================================================================

// Generate cryptographically secure random bytes
std::vector<uint8_t> secureRandom(size_t count);

// Generate random hex string
std::string secureRandomHex(size_t count);

// Generate random alphanumeric string (URL-safe)
std::string secureRandomAlphanumeric(size_t count);

// ============================================================================
// Time Utilities
// ============================================================================

// Format timestamp as RFC 2822 (for HTTP headers)
std::string formatRfc2822(const std::chrono::system_clock::time_point& tp);

// Format timestamp as ISO 8601
std::string formatIso8601(const std::chrono::system_clock::time_point& tp);

// Parse RFC 2822 timestamp
std::optional<std::chrono::system_clock::time_point> parseRfc2822(std::string_view str);

// Get current timestamp as Unix epoch
int64_t currentUnixTime();

// ============================================================================
// Path Utilities
// ============================================================================

// Normalize path (remove . and .., fix slashes)
std::string normalizePath(std::string_view path);

// Get directory from path
std::string_view directoryOf(std::string_view path);

// Get filename from path
std::string_view filenameOf(std::string_view path);

// Get file extension (including dot)
std::string_view extensionOf(std::string_view path);

// Check if path is absolute
bool isAbsolute(std::string_view path);

// ============================================================================
// MIME Type Detection
// ============================================================================

// Get MIME type from file extension
std::string mimeType(std::string_view path);

// Get MIME type from file extension (uppercase result)
std::string mimeTypeUpper(std::string_view path);

// ============================================================================
// IP Address Utilities
// ============================================================================

// Check if string is a valid IPv4 address
bool isValidIpv4(std::string_view addr);

// Check if string is a valid IPv6 address
bool isValidIpv6(std::string_view addr);

// Check if string is a valid IP address (v4 or v6)
bool isValidIp(std::string_view addr);

// Check if IP matches CIDR notation (e.g., "192.168.1.0/24")
bool matchesCidr(std::string_view ip, std::string_view cidr);

// ============================================================================
// Misc Utilities
// ============================================================================

// Constant-time string comparison (for secrets/tokens)
bool constantTimeEqual(std::string_view a, std::string_view b);

// Hash combine (for std::unordered_map with composite keys)
size_t hashCombine(size_t h1, size_t h2);

// Read entire file into memory
std::optional<std::vector<uint8_t>> readFile(const std::string& path);

// Write data to file
bool writeFile(const std::string& path, std::span<const uint8_t> data);

// Escape HTML entities
std::string escapeHtml(std::string_view str);

// Unescape HTML entities
std::string unescapeHtml(std::string_view str);

// ============================================================================
// Template Implementations (inline for performance)
// ============================================================================

// Case-insensitive string comparison
inline bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Check if string contains substring (case-insensitive)
inline bool icontains(std::string_view haystack, std::string_view needle) {
    return std::search(haystack.begin(), haystack.end(),
                       needle.begin(), needle.end(),
                       [](char a, char b) {
                           return std::tolower(static_cast<unsigned char>(a)) ==
                                  std::tolower(static_cast<unsigned char>(b));
                       }) != haystack.end();
}

// ============================================================================
// Stream Helpers
// ============================================================================

// Format bytes as human-readable (KB, MB, GB, etc.)
std::string formatBytes(size_t bytes);

// Format duration as human-readable
std::string formatDuration(std::chrono::seconds duration);

// ============================================================================
// HMAC-SHA256 (using OpenSSL)
// ============================================================================

std::string hmacSha256(std::string_view key, std::string_view message);

} // namespace OmniNet::Utils

#endif // OMNINET_UTILS_H
