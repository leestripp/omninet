// OmniNet - Non-blocking UDP socket for game networking
#include "net/UdpSocket.h"

#include <cerrno>
#include <cstring>

namespace OmniNet {

// ============================================================================
// Helpers
// ============================================================================

static SocketError mapErrno(int e) {
    switch (e) {
        case EADDRINUSE:    return SocketError::AddressInUse;
        case EACCES:
        case EPERM:         return SocketError::PermissionDenied;
        case ECONNREFUSED:  return SocketError::ConnectionRefused;
        case ECONNRESET:    return SocketError::ConnectionReset;
        case ETIMEDOUT:     return SocketError::Timeout;
        case ENETUNREACH:   return SocketError::NetworkUnreachable;
        case EHOSTUNREACH:  return SocketError::HostUnreachable;
        case EMSGSIZE:      return SocketError::MessageTooLong;
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
        case EAGAIN:        return SocketError::WouldBlock;
        default:            return SocketError::SystemError;
    }
}

static bool setNonBlocking(SOCKET fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// ============================================================================
// Lifecycle
// ============================================================================

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : m_fd(other.m_fd), m_localPort(other.m_localPort) {
    other.m_fd = INVALID_SOCKET;
    other.m_localPort = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        m_fd = other.m_fd;
        m_localPort = other.m_localPort;
        other.m_fd = INVALID_SOCKET;
        other.m_localPort = 0;
    }
    return *this;
}

Result<void, SocketError> UdpSocket::open() {
    if (isOpen()) return std::unexpected(SocketError::AlreadyConnected);

    m_fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_fd == INVALID_SOCKET)
        return std::unexpected(mapErrno(errno));

    if (!setNonBlocking(m_fd)) {
        close();
        return std::unexpected(SocketError::SystemError);
    }
    return {};
}

Result<void, SocketError> UdpSocket::bind(const std::string& address, uint16_t port) {
    if (isOpen()) return std::unexpected(SocketError::AlreadyConnected);

    auto res = open();
    if (!res) return res;

    // Reuse address so quick server restarts do not fail
    int opt = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        close();
        return std::unexpected(SocketError::InvalidAddress);
    }

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        SocketError err = mapErrno(errno);
        close();
        return std::unexpected(err);
    }

    // Port 0 = ephemeral; read back the assigned port
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&bound), &len) == 0)
        m_localPort = ntohs(bound.sin_port);
    else
        m_localPort = port;
    return {};
}

void UdpSocket::close() {
    if (m_fd != INVALID_SOCKET) {
#ifdef _WIN32
        ::closesocket(m_fd);
#else
        ::close(m_fd);
#endif
        m_fd = INVALID_SOCKET;
    }
    m_localPort = 0;
}

bool UdpSocket::isOpen() const {
    return m_fd != INVALID_SOCKET;
}

uint16_t UdpSocket::localPort() const {
    return m_localPort;
}

// ============================================================================
// I/O
// ============================================================================

Result<size_t, SocketError>
UdpSocket::sendTo(std::span<const uint8_t> data, const UdpEndpoint& to) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(to.port);
    if (::inet_pton(AF_INET, to.address.c_str(), &addr.sin_addr) != 1)
        return std::unexpected(SocketError::InvalidAddress);

    ssize_t sent = ::sendto(m_fd,
                            reinterpret_cast<const char*>(data.data()),
                            data.size(), 0,
                            reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent == SOCKET_ERROR)
        return std::unexpected(mapErrno(errno));
    return static_cast<size_t>(sent);
}

Result<UdpDatagram, SocketError> UdpSocket::recvFrom(size_t maxSize) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);

    UdpDatagram dg;
    dg.data.resize(maxSize);

    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    ssize_t n = ::recvfrom(m_fd, reinterpret_cast<char*>(dg.data.data()),
                           maxSize, 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n == SOCKET_ERROR) {
        dg.data.clear();
        return std::unexpected(mapErrno(errno));
    }

    dg.data.resize(static_cast<size_t>(n));
    char buf[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &from.sin_addr, buf, sizeof(buf));
    dg.from.address = buf;
    dg.from.port = ntohs(from.sin_port);
    return dg;
}

bool UdpSocket::waitForData(int timeoutMs) const {
    if (!isOpen()) return false;
#ifdef _WIN32
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_fd, &readSet);
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return ::select(0, &readSet, nullptr, nullptr, &tv) > 0;
#else
    pollfd pfd{};
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    int rc;
    do {
        rc = ::poll(&pfd, 1, timeoutMs);
    } while (rc < 0 && errno == EINTR);
    return rc > 0 && (pfd.revents & POLLIN);
#endif
}

// ============================================================================
// Options
// ============================================================================

Result<void, SocketError> UdpSocket::setReceiveBufferSize(int bytes) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);
    if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char*>(&bytes), sizeof(bytes)) != 0)
        return std::unexpected(mapErrno(errno));
    return {};
}

Result<void, SocketError> UdpSocket::setSendBufferSize(int bytes) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);
    if (::setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF,
                     reinterpret_cast<const char*>(&bytes), sizeof(bytes)) != 0)
        return std::unexpected(mapErrno(errno));
    return {};
}

Result<void, SocketError> UdpSocket::setBroadcast(bool enable) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);
    int opt = enable ? 1 : 0;
    if (::setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST,
                     reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
        return std::unexpected(mapErrno(errno));
    return {};
}

Result<UdpEndpoint, SocketError>
UdpSocket::resolve(const std::string& host, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0 || !result)
        return std::unexpected(SocketError::InvalidAddress);

    auto* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char buf[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
    UdpEndpoint ep{buf, ntohs(addr->sin_port)};
    ::freeaddrinfo(result);
    return ep;
}

} // namespace OmniNet
