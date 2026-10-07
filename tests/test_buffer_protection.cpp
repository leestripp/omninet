// Test for buffer overflow protection in Socket recv operations
#include <iostream>
#include <algorithm>
#include <atomic>
#include <thread>
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
    
    // Test 5: send() with a span larger than RECV_BUFFER_MAX must NOT throw
    // (v1.0.4 fix, bug #8 / ChimpFlow report #2). It clamps per call and
    // sendAll() makes progress — previously this threw 'Invalid send
    // buffer', capping forwarded request bodies at 16MB.
    {
        Socket listener;
        listener.bind("127.0.0.1", 0);
        listener.listen(1);
        uint16_t port = listener.localPort();

        Socket client;
        client.connect("127.0.0.1", port);
        auto server = listener.accept();
        assert(server && server->isOpen());

        const size_t payloadSize = Socket::maxRecvSize() + 1;  // 16MB + 1
        std::vector<uint8_t> payload(payloadSize);
        for (size_t i = 0; i < payloadSize; ++i) {
            payload[i] = static_cast<uint8_t>(i * 31 + 7);
        }

        // Receive concurrently: 16MB exceeds the kernel socket buffers,
        // so a blocking send would deadlock if nobody drained meanwhile.
        std::vector<uint8_t> received(payloadSize);
        std::atomic<size_t> totalReceived{0};
        std::thread receiver([&]() {
            while (totalReceived.load() < payloadSize) {
                size_t done = totalReceived.load();
                size_t want = std::min(payloadSize - done, Socket::maxRecvSize());
                size_t n = server->recv(
                    std::span<uint8_t>(received.data() + done, want));
                if (n == 0) break;
                totalReceived += n;
            }
        });

        // One send() call with an oversized span: must not throw.
        size_t totalSent = client.send(
            std::span<const uint8_t>(payload.data(), payload.size()));
        assert(totalSent > 0 && "send() returned 0 on a live socket");
        // Drain the remainder exactly like sendAll() does.
        while (totalSent < payloadSize) {
            size_t n = client.send(
                std::span<const uint8_t>(payload.data() + totalSent, payloadSize - totalSent));
            if (n == 0) break;
            totalSent += n;
        }
        assert(totalSent == payloadSize && "failed to send full payload");

        receiver.join();
        assert(totalReceived.load() == payloadSize && "did not receive full payload");
        assert(received == payload && "payload corrupted in transit");
        std::cout << "  PASS: send() accepts spans over RECV_BUFFER_MAX (16MB+1 loopback)\n";
    }

    std::cout << "\nAll buffer overflow protection tests passed!\n";
    return 0;
}