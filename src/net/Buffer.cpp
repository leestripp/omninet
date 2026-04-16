// OmniNet - Byte Buffer Implementation
#include "net/Buffer.h"

#include <iomanip>
#include <sstream>

namespace OmniNet {

// ============================================================================
// Buffer Utilities Implementation
// ============================================================================

namespace BufferUtil {

std::string toHex(const ByteBuffer& buffer, size_t maxBytes) {
    size_t toShow = buffer.size();
    if (maxBytes > 0 && maxBytes < toShow) {
        toShow = maxBytes;
    }
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < toShow; ++i) {
        if (i > 0) {
            if (i % 16 == 0) {
                oss << "\n";
            } else if (i % 2 == 0) {
                oss << " ";
            }
        }
        oss << std::setw(2) << static_cast<int>(buffer.data()[i]);
    }
    
    if (buffer.size() > toShow) {
        oss << " ... (" << (buffer.size() - toShow) << " more bytes)";
    }
    
    return oss.str();
}

ByteBuffer fromHex(const std::string& hex) {
    ByteBuffer buffer;
    
    // Remove whitespace
    std::string clean;
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            clean += c;
        }
    }
    
    // Must have even number of characters
    if (clean.length() % 2 != 0) {
        throw BufferException("Invalid hex string: odd length");
    }
    
    for (size_t i = 0; i < clean.length(); i += 2) {
        std::string byteStr = clean.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
            buffer.writeByte(byte);
        } catch (const std::exception&) {
            throw BufferException("Invalid hex character");
        }
    }
    
    return buffer;
}

bool isTextual(const ByteBuffer& buffer, size_t checkBytes) {
    size_t toCheck = buffer.size();
    if (checkBytes > 0 && checkBytes < toCheck) {
        toCheck = checkBytes;
    }
    
    for (size_t i = 0; i < toCheck; ++i) {
        uint8_t c = buffer.data()[i];
        // Check for null bytes or other non-printable control chars (except tab, newline, carriage return)
        if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) {
            return false;
        }
    }
    
    return true;
}

std::string toPrintableString(const ByteBuffer& buffer, size_t maxBytes) {
    std::string result;
    
    size_t toShow = buffer.size();
    if (maxBytes > 0 && maxBytes < toShow) {
        toShow = maxBytes;
    }
    
    result.reserve(toShow);
    
    for (size_t i = 0; i < toShow; ++i) {
        uint8_t c = buffer.data()[i];
        if (c >= 32 && c < 127) {
            result += static_cast<char>(c);
        } else if (c == '\t' || c == '\n' || c == '\r') {
            result += static_cast<char>(c);
        } else {
            result += '.';
        }
    }
    
    if (buffer.size() > toShow) {
        result += "...";
    }
    
    return result;
}

uint32_t checksum(const ByteBuffer& buffer) {
    uint32_t sum = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        sum += buffer.data()[i];
    }
    return sum;
}

} // namespace BufferUtil

} // namespace OmniNet
