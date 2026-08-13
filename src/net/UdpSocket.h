// OmniNet - Non-blocking UDP socket for game networking
#ifndef OMNINET_UDP_SOCKET_H
#define OMNINET_UDP_SOCKET_H
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "utils/Expected.h"

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <poll.h>
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

namespace OmniNet {

// ============================================================================
// UdpEndpoint
// ============================================================================

// A resolved IPv4/IPv6 endpoint (address + port)
struct UdpEndpoint {
    std::string address;   // dotted-quad or hostname
    uint16_t    port = 0;

    bool operator==(const UdpEndpoint& other) const {
        return address == other.address && port == other.port;
    }
    bool operator!=(const UdpEndpoint& other) const { return !(*this == other); }

    std::string toString() const { return address + ":" + std::to_string(port); }
};

// A received datagram: payload plus sender endpoint
struct UdpDatagram {
    UdpEndpoint from;
    std::vector<uint8_t> data;
};

// ============================================================================
// UdpSocket
// ============================================================================

// Low-level non-blocking UDP socket.
// Always non-blocking: tryRecvFrom returns SocketError::WouldBlock when no
// datagram is pending. Use waitForData() (poll-based) or pump in the game loop.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    // Non-copyable
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Movable
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // --- Lifecycle ---

    // Create the socket without binding (client use; ephemeral port on first send)
    [[nodiscard]] Result<void, SocketError> open();
    // Create the socket for a specific family (AF_INET / AF_INET6)
    [[nodiscard]] Result<void, SocketError> open(int family);

    // Create the socket and bind to address:port (server use).
    // "0.0.0.0" (or empty) = all IPv4 interfaces; "::" = dual-stack
    // (IPv6 socket with V6ONLY off, accepts IPv4 clients as v4-mapped);
    // an IPv6 literal binds that address.
    [[nodiscard]] Result<void, SocketError> bind(const std::string& address, uint16_t port);

    void close();
    bool isOpen() const;

    // Local port after bind/open (0 if not yet assigned)
    uint16_t localPort() const;

    // --- I/O ---

    // Send a datagram to an endpoint (non-blocking)
    [[nodiscard]] Result<size_t, SocketError>
    sendTo(std::span<const uint8_t> data, const UdpEndpoint& to);

    // Receive a datagram. Returns SocketError::WouldBlock if none pending.
    [[nodiscard]] Result<UdpDatagram, SocketError> recvFrom(size_t maxSize = 65536);

    // Wait until a datagram is available or timeout elapses.
    // Returns true if readable, false on timeout.
    bool waitForData(int timeoutMs) const;

    // --- Options ---

    [[nodiscard]] Result<void, SocketError> setReceiveBufferSize(int bytes);
    [[nodiscard]] Result<void, SocketError> setSendBufferSize(int bytes);
    [[nodiscard]] Result<void, SocketError> setBroadcast(bool enable);

    // Resolve host:port into an endpoint (no socket required)
    [[nodiscard]] static Result<UdpEndpoint, SocketError>
    resolve(const std::string& host, uint16_t port);

private:
    SOCKET m_fd = INVALID_SOCKET;
    uint16_t m_localPort = 0;
    int m_family = AF_INET;
};

} // namespace OmniNet

#endif // OMNINET_UDP_SOCKET_H
