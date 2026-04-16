// OmniNet WebSocket Tests
#include "websocket/WebSocketFrame.h"
#include <iostream>

using namespace OmniNet::WebSocket;

bool test_frame_text_encoding() {
    Frame frame;
    frame.opcode = Opcode::TEXT;
    frame.payload = std::vector<uint8_t>{'H', 'e', 'l', 'l', 'o'};
    
    auto encoded = FrameCodec::encode(frame, true);
    
    // First byte should be FIN + TEXT opcode (0x81)
    if (encoded.empty() || encoded[0] != 0x81) return false;
    
    // Second byte should be length (0x05)
    if (encoded[1] != 0x05) return false;
    
    return true;
}

bool test_frame_binary_encoding() {
    Frame frame;
    frame.opcode = Opcode::BINARY;
    frame.payload = std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03};
    
    auto encoded = FrameCodec::encode(frame, true);
    
    // First byte should be FIN + BINARY opcode (0x82)
    if (encoded.empty() || encoded[0] != 0x82) return false;
    
    return true;
}

bool test_frame_text_decoding() {
    // Manually create a text frame: FIN+TEXT(0x81), len=5, "Hello"
    std::vector<uint8_t> data = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    
    size_t consumed = 0;
    auto frame = FrameCodec::decode(data, consumed);
    
    if (!frame) return false;
    if (frame->opcode != Opcode::TEXT) return false;
    if (frame->payload.size() != 5) return false;
    
    std::string text(frame->payload.begin(), frame->payload.end());
    if (text != "Hello") return false;
    
    return true;
}

bool test_frame_fragmentation() {
    // Create first fragment (FIN=0, TEXT)
    Frame frame1;
    frame1.fin = false;
    frame1.opcode = Opcode::TEXT;
    frame1.payload = std::vector<uint8_t>{'H', 'e', 'l'};
    
    // Create continuation fragment (FIN=1, CONTINUATION)
    Frame frame2;
    frame2.fin = true;
    frame2.opcode = Opcode::CONTINUATION;
    frame2.payload = std::vector<uint8_t>{'l', 'o'};
    
    auto encoded1 = FrameCodec::encode(frame1, true);
    auto encoded2 = FrameCodec::encode(frame2, true);
    
    // First fragment: FIN=0 + TEXT = 0x01
    if (encoded1[0] != 0x01) return false;
    
    // Continuation: FIN=1 + CONTINUATION = 0x80
    if (encoded2[0] != 0x80) return false;
    
    return true;
}

bool test_frame_close() {
    Frame frame;
    frame.opcode = Opcode::CLOSE;
    frame.payload = std::vector<uint8_t>{0x03, 0xE8};  // 1000
    
    auto encoded = FrameCodec::encode(frame, true);
    
    // First byte should be FIN + CLOSE (0x88)
    if (encoded.empty() || encoded[0] != 0x88) return false;
    
    return true;
}

bool test_frame_ping_pong() {
    Frame ping;
    ping.opcode = Opcode::PING;
    ping.payload = std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78};
    
    auto encoded = FrameCodec::encode(ping, true);
    
    // First byte should be FIN + PING (0x89)
    if (encoded.empty() || encoded[0] != 0x89) return false;
    
    // Auto-generate pong
    auto pong = FrameCodec::pong(ping.payload);
    auto pongEncoded = FrameCodec::encode(pong, true);
    
    // Pong should have opcode PONG (0xA)
    if (pongEncoded.empty() || (pongEncoded[0] & 0x0F) != 0x0A) return false;
    
    return true;
}

int main() {
    std::cout << "Running WebSocket tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_frame_text_encoding()) {
        std::cout << "[PASS] Frame text encoding" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame text encoding" << std::endl;
        failed++;
    }
    
    if (test_frame_binary_encoding()) {
        std::cout << "[PASS] Frame binary encoding" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame binary encoding" << std::endl;
        failed++;
    }
    
    if (test_frame_text_decoding()) {
        std::cout << "[PASS] Frame text decoding" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame text decoding" << std::endl;
        failed++;
    }
    
    if (test_frame_fragmentation()) {
        std::cout << "[PASS] Frame fragmentation" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame fragmentation" << std::endl;
        failed++;
    }
    
    if (test_frame_close()) {
        std::cout << "[PASS] Frame close" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame close" << std::endl;
        failed++;
    }
    
    if (test_frame_ping_pong()) {
        std::cout << "[PASS] Frame ping/pong" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Frame ping/pong" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "WebSocket tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}