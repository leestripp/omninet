// OmniNet Socket Tests
#include "net/Socket.h"
#include "net/TLSSocket.h"

#include <iostream>
#include <memory>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace OmniNet;

bool test_socket_creation() {
    Socket socket;
    if (socket.fd() != -1) return false;
    if (socket.isOpen()) return false;
    
    return true;
}

bool test_socket_invalid() {
    // Create socket with invalid fd
    Socket socket(INVALID_SOCKET);
    
    if (socket.isOpen()) return false;
    
    return true;
}

#ifndef _WIN32
bool test_tls_wrapped_socket_has_single_owner() {
    int sockets[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return false;

    const int wrappedFd = sockets[0];
    bool intact = false;
    {
        auto accepted = std::make_unique<Socket>(wrappedFd);
        auto tls = std::make_unique<TLSSocket>(std::move(accepted));
        tls->close();

        // Reuse the just-closed number deterministically. Destroying the TLS
        // wrapper must not close this unrelated descriptor a second time.
        if (::dup2(sockets[1], wrappedFd) == wrappedFd) {
            tls.reset();
            intact = ::fcntl(wrappedFd, F_GETFD) >= 0;
            ::close(wrappedFd);
        }
    }
    ::close(sockets[1]);
    return intact;
}
#endif

int main() {
    std::cout << "Running Socket tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    // Note: Full socket tests require a server to connect to
    // These are basic unit tests that don't require network
    
    if (test_socket_creation()) {
        std::cout << "[PASS] Socket creation" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Socket creation" << std::endl;
        failed++;
    }
    
    if (test_socket_invalid()) {
        std::cout << "[PASS] Socket with invalid fd" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Socket with invalid fd" << std::endl;
        failed++;
    }
    
#ifndef _WIN32
    if (test_tls_wrapped_socket_has_single_owner()) {
        std::cout << "[PASS] TLS-wrapped socket has a single fd owner" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] TLS-wrapped socket has a single fd owner" << std::endl;
        failed++;
    }
#endif

    std::cout << std::endl;
    std::cout << "Note: Full socket tests require network connectivity" << std::endl;
    std::cout << "Socket tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}