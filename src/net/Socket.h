// OmniNet - Socket wrapper for TCP/IP networking
#ifndef OMNINET_SOCKET_H
#define OMNINET_SOCKET_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "utils/Exception.h"
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
// Socket Exception (defined in Exception.h)
// ============================================================================

// ============================================================================
// Constants
// ============================================================================

// Maximum receive buffer size to prevent buffer overflow attacks
// Requests larger than this will be rejected
static constexpr size_t RECV_BUFFER_MAX = 16 * 1024 * 1024;  // 16MB max single recv

// ============================================================================
// Base Socket Class
// ============================================================================

class Socket {
public:
    // Construct an empty socket (for accept())
    Socket();
    
    // Wrap an existing file descriptor
    explicit Socket(SOCKET fd);
    
    // Get the maximum allowed receive buffer size
    static constexpr size_t maxRecvSize() { return RECV_BUFFER_MAX; }
    
    virtual ~Socket();
    
    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    // --- Connection Operations ---

    // Connect to a host:port (TCP)
    virtual void connect(const std::string& host, uint16_t port);

    // Connect with type-safe error handling (Phase 8c)
    [[nodiscard]] virtual OmniNet::Result<void, OmniNet::SocketError>
    tryConnect(const std::string& host, uint16_t port);

    // Bind to address:port (for server)
    virtual void bind(const std::string& address, uint16_t port);

    // Bind with type-safe error handling (Phase 8c)
    [[nodiscard]] virtual OmniNet::Result<void, OmniNet::SocketError>
    tryBind(const std::string& address, uint16_t port);

    // Listen for connections
    virtual void listen(int backlog = 128);

    // Accept incoming connection
    virtual std::unique_ptr<Socket> accept();
    
    // --- I/O Operations ---
    
    // Send data (returns bytes sent)
    virtual size_t send(std::span<const uint8_t> data);
    virtual size_t send(std::string_view data);
    
    // Send all data (until complete or error)
    virtual void sendAll(std::span<const uint8_t> data);
    
    // Receive data (returns bytes received, 0 = connection closed)
    virtual size_t recv(std::span<uint8_t> buffer);
    
    // Receive all data until connection closes or buffer full
    virtual std::vector<uint8_t> recvAll(size_t maxSize = 65536);
    
    // Receive exactly n bytes (blocking)
    virtual std::vector<uint8_t> recvExactly(size_t n);
    
    // Read line (including newline)
    virtual std::optional<std::string> readLine(size_t maxLength = 8192);
    
    // --- State ---
    
    // Check if socket is open
    virtual bool isOpen() const;
    
    // Close the socket
    virtual void close();

    // Shutdown the socket (SHUT_RD, SHUT_WR, or SHUT_RDWR).
    // Useful for interrupting blocking accept() calls.
    void shutdown(int how);
    
    // Get underlying file descriptor
    SOCKET fd() const;
    
    // Get peer address as string
    std::string peerAddress() const;
    
    // Get peer port
    uint16_t peerPort() const;
    
    // Get local address
    std::string localAddress() const;
    
    // Get local port
    uint16_t localPort() const;
    
    // --- Options ---
    
    // Set non-blocking mode
    void setNonBlocking(bool enabled);
    bool isNonBlocking() const;
    
    // Set timeout for receive operations
    void setRecvTimeout(std::chrono::milliseconds timeout);
    
    // Set timeout for send operations
    void setSendTimeout(std::chrono::milliseconds timeout);
    
    // Enable address reuse (SO_REUSEADDR)
    void setReuseAddr(bool enabled);
    
    // Enable TCP no-delay (disable Nagle's algorithm)
    void setTcpNoDelay(bool enabled);
    
    // Set receive buffer size
    void setRecvBufferSize(size_t size);
    
    // Set send buffer size
    void setSendBufferSize(size_t size);
    
    // --- Waiting ---
    
    // Wait until data is available to read
    virtual bool waitForRead(std::chrono::milliseconds timeout) const;
    
    // Wait until socket is ready to write
    bool waitForWrite(std::chrono::milliseconds timeout) const;
    
    // Wait until socket is ready for I/O or timeout
    bool waitForIO(std::chrono::milliseconds timeout) const;
    
    // Poll for read readiness (cross-platform wrapper)
    bool pollRead(int timeoutMs) const;
    
protected:
    // Helper to get last error
    static int getError();
    
    // Helper to check if error is "would block"
    static bool wouldBlock();
    
    // Set socket option
    template<typename T>
    void setOption(int level, int option, T value);
    
    // Get socket option
    template<typename T>
    T getOption(int level, int option);
    
    SOCKET m_fd = INVALID_SOCKET;
    bool m_ownsFd = true;
    bool m_nonBlocking = false;
};

// ============================================================================
// TCP Server Socket
// ============================================================================

class ServerSocket : public Socket {
public:
    ServerSocket();
    
    // Bind and listen on the specified port
    void bindAndListen(uint16_t port, const std::string& address = "0.0.0.0", int backlog = 128);
    
    // Accept with timeout (nullopt if timeout)
    std::unique_ptr<Socket> acceptWithTimeout(std::chrono::milliseconds timeout);

    // v0.7.0: Accept with wakeup — polls BOTH the listening socket
    // AND `wakeupFd`. Returns nullptr in three cases:
    //   1. timeout expired (no connection, no wakeup)
    //   2. wakeupFd became readable (caller should drain it)
    //   3. spurious wakeup
    // Returns the accepted client socket only if the listen fd was
    // actually ready. Used by Server::runAcceptLoop() to implement
    // the self-pipe trick for instant clean shutdown — stop() writes
    // to wakeupFd and the loop wakes up immediately.
    std::unique_ptr<Socket> acceptOrWakeup(int wakeupFd,
                                           std::chrono::milliseconds timeout);

    // Set if kernel should prefer TIME_WAIT recycling
    void setReusePort(bool enabled);
};

// ============================================================================
// Address Resolution
// ============================================================================

namespace Network {

// Resolve hostname to IP address
std::optional<std::string> resolveHost(const std::string& hostname);

// Resolve hostname to all available addresses
std::vector<std::string> resolveHostAll(const std::string& hostname);

// Get address family (AF_INET for IPv4, AF_INET6 for IPv6)
int getAddressFamily(const std::string& address);

// Convert address to string
std::string addressToString(const struct sockaddr* addr, socklen_t addrlen);

// Convert string to sockaddr_storage
bool stringToAddress(const std::string& address, uint16_t port,
                     struct sockaddr_storage* result);

// Check if address is IPv4
bool isIpv4(const std::string& address);

// Check if address is IPv6
bool isIpv6(const std::string& address);

// Check if address is loopback
bool isLoopback(const std::string& address);

// Check if address is private (10.x, 172.16.x, 192.168.x, etc.)
bool isPrivate(const std::string& address);

} // namespace Network

// ============================================================================
// Inline Template Implementations
// ============================================================================

template<typename T>
inline void Socket::setOption(int level, int option, T value) {
#ifdef _WIN32
    ::setsockopt(m_fd, level, option, reinterpret_cast<const char*>(&value), sizeof(value));
#else
    ::setsockopt(m_fd, level, option, &value, sizeof(value));
#endif
}

template<typename T>
inline T Socket::getOption(int level, int option) {
    T value{};
    socklen_t len = sizeof(value);
#ifdef _WIN32
    ::getsockopt(m_fd, level, option, reinterpret_cast<char*>(&value), &len);
#else
    ::getsockopt(m_fd, level, option, &value, &len);
#endif
    return value;
}

// ============================================================================
// Convenience: Create connected socket
// ============================================================================

// Create and connect a socket in one call
std::unique_ptr<Socket> createConnectedSocket(const std::string& host, uint16_t port,
                                               std::chrono::milliseconds timeout = {});

} // namespace OmniNet

#endif // OMNINET_SOCKET_H
