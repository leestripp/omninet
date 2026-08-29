// OmniNet - Socket Implementation
#include "Socket.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace OmniNet {

// ============================================================================
// Socket Base Class
// ============================================================================

Socket::Socket() : m_fd(INVALID_SOCKET), m_ownsFd(true) {}

Socket::Socket(SOCKET fd) : m_fd(fd), m_ownsFd(true) {}

Socket::~Socket() {
    if (m_fd != INVALID_SOCKET && m_ownsFd) {
        // Explicit base-class call: virtual dispatch is disabled in
        // destructors anyway, and this is the Socket implementation we want.
        Socket::close();
    }
}

Socket::Socket(Socket&& other) noexcept
    : m_fd(other.m_fd)
    , m_ownsFd(other.m_ownsFd)
    , m_nonBlocking(other.m_nonBlocking) {
    other.m_fd = INVALID_SOCKET;
    other.m_ownsFd = false;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (m_fd != INVALID_SOCKET && m_ownsFd) {
            ::close(m_fd);
        }
        m_fd = other.m_fd;
        m_ownsFd = other.m_ownsFd;
        m_nonBlocking = other.m_nonBlocking;
        other.m_fd = INVALID_SOCKET;
        other.m_ownsFd = false;
    }
    return *this;
}

void Socket::connect(const std::string& host, uint16_t port) {
    auto result = tryConnect(host, port);
    if (!result) {
        throw SocketException("Failed to connect to " + host + ":" +
                              std::to_string(port), 0);
    }
}

OmniNet::Result<void, OmniNet::SocketError>
Socket::tryConnect(const std::string& host, uint16_t port) {
    struct addrinfo hints, *result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    int error = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (error != 0) {
        // Could not resolve host
        if (error == EAI_NONAME) {
            return std::unexpected<OmniNet::SocketError>{
                OmniNet::SocketError::HostUnreachable};
        }
        return std::unexpected<OmniNet::SocketError>{
            OmniNet::SocketError::InvalidAddress};
    }

    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        m_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (m_fd == INVALID_SOCKET) continue;

        if (::connect(m_fd, rp->ai_addr, static_cast<socklen_t>(rp->ai_addrlen)) == 0) {
            break;
        }

        ::close(m_fd);
        m_fd = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (m_fd == INVALID_SOCKET) {
        // Map errno to SocketError
        OmniNet::SocketError err = OmniNet::SocketError::Unknown;
        switch (errno) {
            case ECONNREFUSED:  err = OmniNet::SocketError::ConnectionRefused; break;
            case ETIMEDOUT:     err = OmniNet::SocketError::Timeout; break;
            case ENETUNREACH:   err = OmniNet::SocketError::NetworkUnreachable; break;
            case EHOSTUNREACH:  err = OmniNet::SocketError::HostUnreachable; break;
            case EACCES:
            case EPERM:         err = OmniNet::SocketError::PermissionDenied; break;
            default:            err = OmniNet::SocketError::Unknown; break;
        }
        return std::unexpected<OmniNet::SocketError>{err};
    }

    return {};  // Success
}

void Socket::bind(const std::string& address, uint16_t port) {
    auto result = tryBind(address, port);
    if (!result) {
        throw SocketException("Failed to bind to " + address + ":" +
                              std::to_string(port), getError());
    }
}

OmniNet::Result<void, OmniNet::SocketError>
Socket::tryBind(const std::string& address, uint16_t port) {
    if (m_fd == INVALID_SOCKET) {
        m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_fd == INVALID_SOCKET) {
            return std::unexpected<OmniNet::SocketError>{
                OmniNet::SocketError::NotASocket};
        }
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return std::unexpected<OmniNet::SocketError>{
                OmniNet::SocketError::InvalidAddress};
        }
    }

    if (::bind(m_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        OmniNet::SocketError err = OmniNet::SocketError::Unknown;
        switch (errno) {
            case EADDRINUSE:  err = OmniNet::SocketError::AddressInUse; break;
            case EACCES:
            case EPERM:       err = OmniNet::SocketError::PermissionDenied; break;
            default:          err = OmniNet::SocketError::Unknown; break;
        }
        return std::unexpected<OmniNet::SocketError>{err};
    }

    return {};  // Success
}

void Socket::listen(int backlog) {
    if (::listen(m_fd, backlog) < 0) {
        throw SocketException(std::string("Failed to listen"), getError());
    }
}

std::unique_ptr<Socket> Socket::accept() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    std::memset(&clientAddr, 0, sizeof(clientAddr));
    SOCKET clientFd = ::accept(m_fd, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
    
    if (clientFd == INVALID_SOCKET) {
        if (wouldBlock()) {
            return nullptr;
        }
        throw SocketException(std::string("Failed to accept connection"), getError());
    }
    
    return std::make_unique<Socket>(clientFd);
}

size_t Socket::send(std::span<const uint8_t> data) {
    // Validate span
    if (data.data() == nullptr || data.size() > RECV_BUFFER_MAX) {
        throw SocketException(std::string("Invalid send buffer"), EINVAL);
    }
    
    ssize_t result = ::send(m_fd, reinterpret_cast<const char*>(data.data()),
                            static_cast<size_t>(data.size()), 0);
    if (result < 0) {
        if (wouldBlock()) return 0;
        throw SocketException(std::string("Send failed"), getError());
    }
    return static_cast<size_t>(result);
}

size_t Socket::send(std::string_view data) {
    return send(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()
    ));
}

void Socket::sendAll(std::span<const uint8_t> data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        size_t sent = send(data.subspan(totalSent));
        if (sent == 0) {
            throw SocketException(std::string("Connection closed during send"));
        }
        totalSent += sent;
    }
}

size_t Socket::recv(std::span<uint8_t> buffer) {
    // Validate span - CRITICAL for buffer overflow protection
    if (buffer.data() == nullptr) {
        throw SocketException(std::string("Null receive buffer"), EINVAL);
    }
    
    // Reject absurdly large spans that could cause issues
    if (buffer.size() > RECV_BUFFER_MAX) {
        std::string msg = "Receive buffer too large: " + std::to_string(buffer.size()) + 
                         " bytes (max: " + std::to_string(RECV_BUFFER_MAX) + ")";
        throw SocketException(msg, ENOBUFS);
    }
    
    ssize_t result = ::recv(m_fd, reinterpret_cast<char*>(buffer.data()),
                           static_cast<size_t>(buffer.size()), 0);
    if (result < 0) {
        if (wouldBlock()) return 0;
        throw SocketException(std::string("Receive failed"), getError());
    }
    return static_cast<size_t>(result);
}

std::vector<uint8_t> Socket::recvAll(size_t maxSize) {
    // Clamp maxSize to prevent buffer overflow and excessive memory allocation
    // This is a defense-in-depth measure against malformed/malicious requests
    if (maxSize == 0) {
        return {};
    }
    
    // Cap at RECV_BUFFER_MAX to prevent allocating huge buffers
    size_t actualSize = std::min(maxSize, RECV_BUFFER_MAX);
    
    std::vector<uint8_t> buffer;
    try {
        buffer.resize(actualSize);
    } catch (const std::bad_alloc&) {
        throw SocketException(std::string("Failed to allocate receive buffer"), ENOMEM);
    }
    
    if (buffer.data() == nullptr) {
        throw SocketException(std::string("Invalid receive buffer (null)"), EINVAL);
    }
    
    // Create span with explicit bounds check
    std::span<uint8_t> recvSpan = std::span<uint8_t>(buffer.data(), buffer.size());
    
    // Validate span is usable before passing to recv()
    if (recvSpan.data() == nullptr || recvSpan.size() > RECV_BUFFER_MAX) {
        throw SocketException(std::string("Invalid span created for receive"), EINVAL);
    }
    
    // Just do a single recv() - don't loop
    // The caller should handle partial reads and looping
    size_t received = recv(recvSpan);
    
    if (received == 0) {
        // Connection closed or no data
        return {};
    }
    
    // Return only the data we received, not the whole buffer
    return std::vector<uint8_t>(buffer.begin(), buffer.begin() + received);
}

std::vector<uint8_t> Socket::recvExactly(size_t n) {
    // Sanity check - don't try to receive absurdly large amounts
    if (n > RECV_BUFFER_MAX) {
        std::string msg = "recvExactly requested too much data: " + std::to_string(n) + 
                         " bytes (max: " + std::to_string(RECV_BUFFER_MAX) + ")";
        throw SocketException(msg, ENOBUFS);
    }
    
    std::vector<uint8_t> buffer(n);
    size_t totalReceived = 0;
    
    while (totalReceived < n) {
        size_t chunkSize = std::min(n - totalReceived, RECV_BUFFER_MAX);
        size_t received = recv(std::span<uint8_t>(buffer.data() + totalReceived, chunkSize));
        if (received == 0) {
            throw SocketException(std::string("Connection closed before receiving all data"));
        }
        totalReceived += received;
    }
    
    return buffer;
}

std::optional<std::string> Socket::readLine(size_t maxLength) {
    // Limit maxLength to prevent buffer issues
    maxLength = std::min(maxLength, RECV_BUFFER_MAX);
    
    std::string line;
    line.reserve(256);
    
    while (line.size() < maxLength) {
        char c = 0;
        ssize_t result = ::recv(m_fd, &c, 1, 0);
        
        if (result <= 0) {
            if (result < 0 && wouldBlock()) {
                if (!line.empty()) {
                    return line;
                }
                return std::nullopt;
            }
            if (line.empty()) {
                return std::nullopt;
            }
            return line;
        }
        
        if (c == '\n') {
            // Remove \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        
        line += c;
    }
    
    return line;
}

bool Socket::hasPeerClosed() const {
    if (!isOpen()) return true;
#ifdef _WIN32
    char b;
    int r = ::recv(m_fd, &b, 1, MSG_PEEK);
    if (r == 0) return true;                    // EOF
    if (r < 0) {
        int e = WSAGetLastError();
        return e != WSAEWOULDBLOCK;             // would-block = alive
    }
    return false;
#else
    uint8_t b;
    ssize_t r = ::recv(m_fd, &b, 1, MSG_PEEK | MSG_DONTWAIT);
    if (r == 0) return true;                    // EOF: peer sent FIN
    if (r < 0) {
        return errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR;
    }
    return false;                               // data pending = alive
#endif
}

bool Socket::isOpen() const {
    return m_fd != INVALID_SOCKET;
}

void Socket::close() {
    if (m_fd != INVALID_SOCKET) {
#ifdef _WIN32
        ::closesocket(m_fd);
#else
        ::close(m_fd);
#endif
        m_fd = INVALID_SOCKET;
    }
}

void Socket::shutdown(int how) {
    if (m_fd != INVALID_SOCKET) {
        ::shutdown(m_fd, how);
    }
}

SOCKET Socket::fd() const {
    return m_fd;
}

std::string Socket::peerAddress() const {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(m_fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }
    char buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer)) == nullptr) {
        return "";
    }
    return buffer;
}

uint16_t Socket::peerPort() const {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(m_fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return 0;
    }
    return ntohs(addr.sin_port);
}

std::string Socket::localAddress() const {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(m_fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }
    char buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer)) == nullptr) {
        return "";
    }
    return buffer;
}

uint16_t Socket::localPort() const {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(m_fd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return 0;
    }
    return ntohs(addr.sin_port);
}

void Socket::setNonBlocking(bool enabled) {
#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    ioctlsocket(m_fd, FIONBIO, &mode);
#else
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (enabled) {
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(m_fd, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
    m_nonBlocking = enabled;
}

bool Socket::isNonBlocking() const {
    return m_nonBlocking;
}

void Socket::setRecvTimeout(std::chrono::milliseconds timeout) {
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(timeout.count());
    setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::setSendTimeout(std::chrono::milliseconds timeout) {
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(timeout.count());
    setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::setReuseAddr(bool enabled) {
    int optval = enabled ? 1 : 0;
    setOption(SOL_SOCKET, SO_REUSEADDR, optval);
}

void Socket::setTcpNoDelay(bool enabled) {
    int optval = enabled ? 1 : 0;
    setOption(IPPROTO_TCP, TCP_NODELAY, optval);
}

void Socket::setRecvBufferSize(size_t size) {
    // Cap at reasonable maximum to prevent DoS
    size = std::min(size, RECV_BUFFER_MAX * 4);
    setOption(SOL_SOCKET, SO_RCVBUF, static_cast<int>(size));
}

void Socket::setSendBufferSize(size_t size) {
    // Cap at reasonable maximum to prevent DoS
    size = std::min(size, RECV_BUFFER_MAX * 4);
    setOption(SOL_SOCKET, SO_SNDBUF, static_cast<int>(size));
}

bool Socket::waitForRead(std::chrono::milliseconds timeout) const {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);
    
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    int result = select(static_cast<int>(m_fd + 1), &fds, nullptr, nullptr, &tv);
    return result > 0;
}

bool Socket::waitForWrite(std::chrono::milliseconds timeout) const {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);
    
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    int result = select(static_cast<int>(m_fd + 1), nullptr, &fds, nullptr, &tv);
    return result > 0;
}

bool Socket::waitForIO(std::chrono::milliseconds timeout) const {
    fd_set readFds, writeFds, errorFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errorFds);
    FD_SET(m_fd, &readFds);
    FD_SET(m_fd, &writeFds);
    FD_SET(m_fd, &errorFds);
    
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    int result = select(static_cast<int>(m_fd + 1), &readFds, &writeFds, &errorFds, &tv);
    return result > 0;
}

bool Socket::pollRead(int timeoutMs) const {
    return waitForRead(std::chrono::milliseconds(timeoutMs));
}

int Socket::getError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool Socket::wouldBlock() {
#ifdef _WIN32
    return getError() == WSAEWOULDBLOCK;
#else
    return getError() == EAGAIN || getError() == EWOULDBLOCK;
#endif
}

// ============================================================================
// Server Socket
// ============================================================================

ServerSocket::ServerSocket() {
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd == INVALID_SOCKET) {
        throw SocketException(std::string("Failed to create socket"), getError());
    }
    setReuseAddr(true);
}

void ServerSocket::bindAndListen(uint16_t port, const std::string& address, int backlog) {
    bind(address, port);
    listen(backlog);
    // v0.7.0 fix (test_websocket_echo hang): make the listening
    // socket non-blocking. Without this, a race between poll()
    // reporting POLLIN and accept() being called can cause
    // accept() to block indefinitely when the pending connection
    // was removed by an async network error (or the test closed
    // the socket). With a non-blocking listening socket,
    // accept() returns EAGAIN immediately in that case, and we
    // continue the accept loop cleanly. See the Linux accept(2)
    // man page: "It is possible to select(2) or poll(2) for
    // completion of an attempted connect(2) by selecting the
    // socket for writing. After select(2) or poll(2) indicates
    // writability, use getsockname(2) to determine whether the
    // connection completed successfully." — and analogously,
    // poll() reporting POLLIN on a listening socket can race
    // with the kernel removing the pending connection.
    setNonBlocking(true);
}

std::unique_ptr<Socket> ServerSocket::acceptWithTimeout(std::chrono::milliseconds timeout) {
    if (waitForRead(timeout)) {
        return accept();
    }
    return nullptr;
}

std::unique_ptr<Socket> ServerSocket::acceptOrWakeup(
    int wakeupFd, std::chrono::milliseconds timeout) {

    // v0.7.0: self-pipe trick. poll() both the listen fd and the
    // wakeup fd. When stop() writes to the wakeup pipe, poll()
    // returns immediately and we exit the accept loop cleanly.
    // The listening socket is non-blocking (see bindAndListen),
    // so accept() never blocks even if the pending connection
    // was removed between poll() and accept().
    struct pollfd fds[2];
    fds[0].fd = m_fd;
    fds[0].events = POLLIN;
    fds[1].fd = wakeupFd;
    fds[1].events = POLLIN;

    int ret = ::poll(fds, 2, static_cast<int>(timeout.count()));
    if (ret < 0) {
        if (errno == EINTR) return nullptr;  // interrupted — caller retries
        throw SocketException(std::string("poll() failed"), getError());
    }
    if (ret == 0) return nullptr;  // timeout

    // Wakeup fd is ready — caller should drain it and check m_running.
    if (fds[1].revents & POLLIN) return nullptr;

    // Listen fd is ready — accept the connection.
    if (fds[0].revents & POLLIN) {
        // v0.7.0 fix (test_websocket_echo hang): handle all
        // the documented accept() error cases. With a
        // non-blocking listening socket:
        //   EAGAIN/EWOULDBLOCK: poll() reported POLLIN but the
        //     connection was removed before accept() — just
        //     return nullptr and let the loop continue.
        //   ECONNABORTED: a pending connection was aborted
        //     (e.g., RST received). Log and continue.
        //   EPERM: a connection was blocked by a firewall rule.
        //     Log and continue.
        // Without this, these errors would either throw
        // (crashing the accept loop) or block (hanging the
        // accept loop and starving other threads).
        SOCKET clientFd = ::accept(m_fd, nullptr, nullptr);
        if (clientFd == INVALID_SOCKET) {
            int err = getError();
            if (err == EAGAIN || err == EWOULDBLOCK) {
                return nullptr;  // spurious readiness
            }
            if (err == ECONNABORTED || err == EPERM || err == ENETDOWN ||
                err == EPROTO || err == ENOPROTOOPT) {
                // Log and continue — don't crash the accept loop
                // for a single bad connection.
                return nullptr;
            }
            throw SocketException(std::string("accept() failed"), err);
        }
        return std::make_unique<Socket>(clientFd);
    }

    return nullptr;  // spurious wakeup, retry
}

void ServerSocket::setReusePort(bool enabled) {
#ifdef SO_REUSEPORT
    int optval = enabled ? 1 : 0;
    setOption(SOL_SOCKET, SO_REUSEPORT, optval);
#endif
}

// ============================================================================
// Network Utilities
// ============================================================================

namespace Network {

std::optional<std::string> resolveHost(const std::string& hostname) {
    struct addrinfo hints, *result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0) {
        return std::nullopt;
    }
    
    std::optional<std::string> addr;
    if (result) {
        char buffer[INET_ADDRSTRLEN];
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
        if (inet_ntop(AF_INET, &addr4->sin_addr, buffer, sizeof(buffer))) {
            addr = buffer;
        }
        freeaddrinfo(result);
    }
    
    return addr;
}

std::vector<std::string> resolveHostAll(const std::string& hostname) {
    struct addrinfo hints, *result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    std::vector<std::string> addresses;
    
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0) {
        return addresses;
    }
    
    char buffer[INET6_ADDRSTRLEN];
    
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            auto* addr4 = reinterpret_cast<struct sockaddr_in*>(rp->ai_addr);
            if (inet_ntop(AF_INET, &addr4->sin_addr, buffer, sizeof(buffer))) {
                addresses.push_back(buffer);
            }
        } else if (rp->ai_family == AF_INET6) {
            auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(rp->ai_addr);
            if (inet_ntop(AF_INET6, &addr6->sin6_addr, buffer, sizeof(buffer))) {
                addresses.push_back(buffer);
            }
        }
    }
    
    freeaddrinfo(result);
    return addresses;
}

int getAddressFamily(const std::string& address) {
    if (isIpv4(address)) return AF_INET;
    if (isIpv6(address)) return AF_INET6;
    return AF_UNSPEC;
}

std::string addressToString(const struct sockaddr* addr, [[maybe_unused]] socklen_t addrlen) {
    char buffer[INET6_ADDRSTRLEN];
    
    if (addr->sa_family == AF_INET) {
        auto* addr4 = reinterpret_cast<const struct sockaddr_in*>(addr);
        inet_ntop(AF_INET, &addr4->sin_addr, buffer, sizeof(buffer));
    } else if (addr->sa_family == AF_INET6) {
        auto* addr6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, buffer, sizeof(buffer));
    } else {
        return "";
    }
    
    return buffer;
}

bool stringToAddress(const std::string& address, uint16_t port,
                     struct sockaddr_storage* result) {
    std::memset(result, 0, sizeof(*result));
    
    if (isIpv4(address)) {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(result);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
        return inet_pton(AF_INET, address.c_str(), &addr4->sin_addr) == 1;
    } else if (isIpv6(address)) {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(result);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        return inet_pton(AF_INET6, address.c_str(), &addr6->sin6_addr) == 1;
    }
    
    return false;
}

bool isIpv4(const std::string& address) {
    struct sockaddr_in addr;
    return inet_pton(AF_INET, address.c_str(), &addr.sin_addr) == 1;
}

bool isIpv6(const std::string& address) {
    struct sockaddr_in6 addr;
    return inet_pton(AF_INET6, address.c_str(), &addr.sin6_addr) == 1;
}

bool isLoopback(const std::string& address) {
    if (isIpv4(address)) {
        struct in_addr addr;
        inet_pton(AF_INET, address.c_str(), &addr);
        return (ntohl(addr.s_addr) & 0xff000000) == 0x7f000000;  // 127.x.x.x
    }
    return address == "::1";
}

bool isPrivate(const std::string& address) {
    if (isIpv4(address)) {
        struct in_addr addr;
        inet_pton(AF_INET, address.c_str(), &addr);
        uint32_t ip = ntohl(addr.s_addr);
        
        // 10.0.0.0/8
        if ((ip & 0xff000000) == 0x0a000000) return true;
        
        // 172.16.0.0/12
        if ((ip & 0xfff00000) == 0xac100000) return true;
        
        // 192.168.0.0/16
        if ((ip & 0xffff0000) == 0xc0a80000) return true;
        
        return false;
    }
    
    // IPv6 private (link-local, unique local)
    return address.starts_with("fe80:") ||
           address.starts_with("fc") ||
           address.starts_with("fd");
}

} // namespace Network

// ============================================================================
// Convenience Function
// ============================================================================

std::unique_ptr<Socket> createConnectedSocket(const std::string& host, uint16_t port,
                                             std::chrono::milliseconds timeout) {
    auto socket = std::make_unique<Socket>();
    socket->setRecvTimeout(timeout);
    socket->setSendTimeout(timeout);
    socket->connect(host, port);
    return socket;
}

} // namespace OmniNet
