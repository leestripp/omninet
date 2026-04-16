// OmniNet Socket Tests
#include "net/Socket.h"
#include <iostream>

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
    
    std::cout << std::endl;
    std::cout << "Note: Full socket tests require network connectivity" << std::endl;
    std::cout << "Socket tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}