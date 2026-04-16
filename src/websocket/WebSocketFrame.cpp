// OmniNet - WebSocket Frame Implementation
#include "WebSocketFrame.h"

#include <algorithm>

namespace OmniNet::WebSocket {

// ============================================================================
// Frame Codec
// ============================================================================

std::vector<uint8_t> FrameCodec::encode(const Frame& frame) {
    std::vector<uint8_t> result;
    
    // First byte: FIN + opcode
    uint8_t first = static_cast<uint8_t>(frame.opcode);
    if (frame.fin) first |= 0x80;
    result.push_back(first);
    
    // Second byte: MASK + payload length
    uint8_t second = 0;
    size_t payloadLen = frame.payload.size();
    
    if (payloadLen < 126) {
        second = static_cast<uint8_t>(payloadLen);
    } else if (payloadLen < 65536) {
        second = 126;
    } else {
        second = 127;
    }
    
    if (frame.masked) {
        second |= 0x80;
        result.push_back(second);
        
        // Extended length
        if (payloadLen < 126) {
            // No extended length
        } else if (payloadLen < 65536) {
            result.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xff));
            result.push_back(static_cast<uint8_t>(payloadLen & 0xff));
        } else {
            for (int i = 7; i >= 0; --i) {
                result.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xff));
            }
        }
        
        // Masking key
        result.insert(result.end(), 
            frame.maskingKey, frame.maskingKey + 4);
        
        // Mask payload
        auto masked = frame.payload;
        for (size_t i = 0; i < masked.size(); ++i) {
            masked[i] ^= frame.maskingKey[i % 4];
        }
        result.insert(result.end(), masked.begin(), masked.end());
    } else {
        result.push_back(second);
        
        // Extended length (unmasked, no key)
        if (payloadLen < 126) {
            // No extended length
        } else if (payloadLen < 65536) {
            result.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xff));
            result.push_back(static_cast<uint8_t>(payloadLen & 0xff));
        } else {
            for (int i = 7; i >= 0; --i) {
                result.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xff));
            }
        }
        
        result.insert(result.end(), frame.payload.begin(), frame.payload.end());
    }
    
    return result;
}

std::optional<Frame> FrameCodec::decode(std::span<const uint8_t> data, size_t& consumed) {
    consumed = 0;
    
    if (data.size() < 2) {
        return std::nullopt;
    }
    
    Frame frame;
    
    // First byte: FIN + opcode
    frame.fin = (data[0] & 0x80) != 0;
    frame.opcode = static_cast<Opcode>(data[0] & 0x0f);
    
    // Second byte: MASK + payload length
    frame.masked = (data[1] & 0x80) != 0;
    size_t payloadLen = data[1] & 0x7f;
    
    size_t offset = 2;
    
    // Extended length
    if (payloadLen == 126) {
        if (data.size() < 4) return std::nullopt;
        payloadLen = (static_cast<size_t>(data[2]) << 8) | data[3];
        offset = 4;
    } else if (payloadLen == 127) {
        if (data.size() < 10) return std::nullopt;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | data[2 + i];
        }
        offset = 10;
    }
    
    // Masking key
    if (frame.masked) {
        if (data.size() < offset + 4) return std::nullopt;
        std::copy(data.begin() + offset, data.begin() + offset + 4, frame.maskingKey);
        offset += 4;
    }
    
    // Payload
    if (data.size() < offset + payloadLen) {
        return std::nullopt;
    }
    
    frame.payload.resize(payloadLen);
    std::copy(data.begin() + offset, data.begin() + offset + payloadLen, frame.payload.begin());
    
    // Unmask if needed
    if (frame.masked) {
        for (size_t i = 0; i < frame.payload.size(); ++i) {
            frame.payload[i] ^= frame.maskingKey[i % 4];
        }
    }
    
    consumed = offset + payloadLen;
    return frame;
}

bool FrameCodec::isValidStart(std::span<const uint8_t> data) {
    return data.size() >= 2 && (data[0] & 0x0f) <= 0x0a;
}

std::optional<size_t> FrameCodec::frameSize(std::span<const uint8_t> data) {
    if (data.size() < 2) return std::nullopt;
    
    size_t size = 2;
    size_t payloadLen = data[1] & 0x7f;
    
    if (payloadLen == 126) {
        if (data.size() < 4) return std::nullopt;
        size = 4;
        payloadLen = (static_cast<size_t>(data[2]) << 8) | data[3];
    } else if (payloadLen == 127) {
        if (data.size() < 10) return std::nullopt;
        size = 10;
    }
    
    if ((data[1] & 0x80) != 0) {
        size += 4; // Masking key
    }
    
    size += payloadLen;
    return size;
}

// ============================================================================
// Close Frame
// ============================================================================

const char* closeCodeReason(CloseCode code) {
    switch (code) {
        case CloseCode::NORMAL: return "Normal closure";
        case CloseCode::GOING_AWAY: return "Going away";
        case CloseCode::PROTOCOL_ERROR: return "Protocol error";
        case CloseCode::UNSUPPORTED_DATA: return "Unsupported data";
        case CloseCode::RESERVED: return "Reserved";
        case CloseCode::NO_CODE: return "No status code";
        case CloseCode::ABNORMAL_CLOSURE: return "Abnormal closure";
        case CloseCode::INVALID_PAYLOAD: return "Invalid frame payload data";
        case CloseCode::POLICY_VIOLATION: return "Policy violation";
        case CloseCode::MESSAGE_TOO_BIG: return "Message too big";
        case CloseCode::MANDATORY_EXT: return "Mandatory extension";
        case CloseCode::INTERNAL_ERROR: return "Internal server error";
        case CloseCode::SERVICE_RESTART: return "Service restart";
        case CloseCode::TRY_AGAIN_LATER: return "Try again later";
        case CloseCode::BAD_GATEWAY: return "Bad gateway";
        case CloseCode::TLS_HANDSHAKE: return "TLS handshake failure";
        default: return "Unknown";
    }
}

CloseFrame parseCloseFrame(std::span<const uint8_t> payload) {
    CloseFrame frame;
    
    if (payload.size() >= 2) {
        frame.code = static_cast<CloseCode>(
            (static_cast<uint16_t>(payload[0]) << 8) | payload[1]
        );
        
        if (payload.size() > 2) {
            frame.reason.assign(
                payload.begin() + 2,
                payload.end()
            );
        }
    }
    
    return frame;
}

std::vector<uint8_t> buildCloseFrame(const CloseFrame& frame) {
    std::vector<uint8_t> result;
    
    uint16_t code = static_cast<uint16_t>(frame.code);
    result.push_back(static_cast<uint8_t>((code >> 8) & 0xff));
    result.push_back(static_cast<uint8_t>(code & 0xff));
    
    if (!frame.reason.empty()) {
        result.insert(result.end(), frame.reason.begin(), frame.reason.end());
    }
    
    return result;
}

} // namespace OmniNet::WebSocket
