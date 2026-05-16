// Test for buffer overflow protection in Socket recv operations
#include <iostream>
#include <cassert>
#include <cstring>
#include "net/Socket.h"
#include "utils/Exception.h"

using namespace OmniNet;

int main() {
    std::cout << "Testing Socket buffer overflow protection...\n";
    
    // Test 1: RECV_BUFFER_MAX constant exists and is reasonable
    std::cout << "  RECV_BUFFER_MAX = " << Socket::maxRecvSize() << " bytes ("
              << (Socket::maxRecvSize() / (1024*1024)) << " MB)\n";
    assert(Socket::maxRecvSize() > 0);
    assert(Socket::maxRecvSize() <= 256 * 1024 * 1024); // Should be <= 256MB
    std::cout << "  PASS: RECV_BUFFER_MAX is reasonable\n";
    
    // Test 2: Creating a Socket and calling recv with huge span should throw
    {
        Socket sock; // Invalid socket, won't actually recv
        
        std::vector<uint8_t> hugeBuffer(Socket::maxRecvSize() + 1);
        std::span<uint8_t> hugeSpan(hugeBuffer.data(), hugeBuffer.size());
        
        bool caughtException = false;
        try {
            sock.recv(hugeSpan);
        } catch (const SocketException& e) {
            caughtException = true;
            std::cout << "  Caught expected exception: " << e.what() << "\n";
        } catch (...) {
            std::cout << "  Caught unexpected exception type\n";
        }
        assert(caughtException && "Should throw SocketException for oversized buffer");
        std::cout << "  PASS: recv() throws on oversized buffer\n";
    }
    
    // Test 3: recvAll with 0 size should return empty
    {
        Socket sock;
        auto result = sock.recvAll(0);
        assert(result.empty() && "recvAll(0) should return empty vector");
        std::cout << "  PASS: recvAll(0) returns empty vector\n";
    }
    
    // Test 4: recvExactly with huge size should throw
    {
        Socket sock;
        
        bool caughtException = false;
        try {
            // Request more than RECV_BUFFER_MAX
            sock.recvExactly(Socket::maxRecvSize() + 1);
        } catch (const SocketException& e) {
            caughtException = true;
            std::cout << "  Caught expected exception: " << e.what() << "\n";
        } catch (...) {
            std::cout << "  Caught unexpected exception type\n";
        }
        assert(caughtException && "Should throw SocketException for huge recvExactly request");
        std::cout << "  PASS: recvExactly() throws on oversized request\n";
    }
    
    std::cout << "\nAll buffer overflow protection tests passed!\n";
    return 0;
}