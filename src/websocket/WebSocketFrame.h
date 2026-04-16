// OmniNet - WebSocket Frame Codec
#ifndef OMNINET_WS_FRAME_H
#define OMNINET_WS_FRAME_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace OmniNet::WebSocket {

// ============================================================================
// WebSocket Opcodes
// ============================================================================

enum class Opcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

// ============================================================================
// WebSocket Frame
// ============================================================================

struct Frame {
    bool fin = true;
    Opcode opcode;
    bool masked = false;  // Client frames must be masked
    uint8_t maskingKey[4] = {0, 0, 0, 0};  // Only for masked frames
    
    // Payload
    std::vector<uint8_t> payload;
    
    // Convenience methods
    bool isText() const { return opcode == Opcode::TEXT; }
    bool isBinary() const { return opcode == Opcode::BINARY; }
    bool isContinuation() const { return opcode == Opcode::CONTINUATION; }
    bool isClose() const { return opcode == Opcode::CLOSE; }
    bool isPing() const { return opcode == Opcode::PING; }
    bool isPong() const { return opcode == Opcode::PONG; }
    bool isControl() const { return opcode >= Opcode::CLOSE; }
};

// ============================================================================
// WebSocket Frame Codec
// ============================================================================

class FrameCodec {
public:
    // Encode a frame for sending
    static std::vector<uint8_t> encode(const Frame& frame);
    
    // Decode frames from data
    // Returns decoded frame and number of bytes consumed
    // Returns nullopt if more data needed
    static std::optional<Frame> decode(std::span<const uint8_t> data, size_t& consumed);
    
    // Check if data starts with a valid frame
    static bool isValidStart(std::span<const uint8_t> data);
    
    // Get frame size from header (without reading payload)
    static std::optional<size_t> frameSize(std::span<const uint8_t> data);

private:
    static size_t encodeHeader(const Frame& frame, uint8_t* buffer);
    static std::optional<Frame> decodeHeader(
        std::span<const uint8_t> data,
        size_t& consumed
    );
};

// ============================================================================
// Close Codes
// ============================================================================

enum class CloseCode : uint16_t {
    NORMAL = 1000,
    GOING_AWAY = 1001,
    PROTOCOL_ERROR = 1002,
    UNSUPPORTED_DATA = 1003,
    RESERVED = 1004,
    NO_CODE = 1005,
    ABNORMAL_CLOSURE = 1006,
    INVALID_PAYLOAD = 1007,
    POLICY_VIOLATION = 1008,
    MESSAGE_TOO_BIG = 1009,
    MANDATORY_EXT = 1010,
    INTERNAL_ERROR = 1011,
    SERVICE_RESTART = 1012,
    TRY_AGAIN_LATER = 1013,
    BAD_GATEWAY = 1014,
    TLS_HANDSHAKE = 1015
};

const char* closeCodeReason(CloseCode code);

// ============================================================================
// Close Frame Parser
// ============================================================================

struct CloseFrame {
    CloseCode code = CloseCode::NORMAL;
    std::string reason;
    
    static CloseFrame normal() { return {CloseCode::NORMAL, ""}; }
    static CloseFrame protocolError() { return {CloseCode::PROTOCOL_ERROR, ""}; }
    static CloseFrame invalidPayload() { return {CloseCode::INVALID_PAYLOAD, ""}; }
};

CloseFrame parseCloseFrame(std::span<const uint8_t> payload);
std::vector<uint8_t> buildCloseFrame(const CloseFrame& frame);

} // namespace OmniNet::WebSocket

#endif // OMNINET_WS_FRAME_H
