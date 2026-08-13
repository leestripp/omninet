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
    : m_fd(other.m_fd), m_localPort(other.m_localPort), m_family(other.m_family) {
    other.m_fd = INVALID_SOCKET;
    other.m_localPort = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        m_fd = other.m_fd;
        m_localPort = other.m_localPort;
        m_family = other.m_family;
        other.m_fd = INVALID_SOCKET;
        other.m_localPort = 0;
    }
    return *this;
}


// Parse an endpoint into a sockaddr_storage. Returns 0 on failure, else the
// sockaddr length. IPv6 literals are detected by the ':' in the address.
socklen_t parseEndpoint(const UdpEndpoint& ep, sockaddr_storage& out) {
    std::memset(&out, 0, sizeof(out));
    if (ep.address.find(':') != std::string::npos) {
        auto* a6 = reinterpret_cast<sockaddr_in6*>(&out);
        a6->sin6_family = AF_INET6;
        a6->sin6_port = htons(ep.port);
        if (::inet_pton(AF_INET6, ep.address.c_str(), &a6->sin6_addr) != 1)
            return 0;
        return sizeof(sockaddr_in6);
    }
    auto* a4 = reinterpret_cast<sockaddr_in*>(&out);
    a4->sin_family = AF_INET;
    a4->sin_port = htons(ep.port);
    if (::inet_pton(AF_INET, ep.address.c_str(), &a4->sin_addr) != 1)
        return 0;
    return sizeof(sockaddr_in);
}

// Convert a sockaddr (v4 or v6) back to an endpoint. v4-mapped IPv6
// addresses (::ffff:a.b.c.d) are normalized to plain IPv4 so dual-stack
// servers see consistent endpoint keys.
UdpEndpoint endpointFromSockAddr(const sockaddr_storage& ss) {
    UdpEndpoint ep;
    char buf[INET6_ADDRSTRLEN]{};
    if (ss.ss_family == AF_INET6) {
        const auto* a6 = reinterpret_cast<const sockaddr_in6*>(&ss);
        ::inet_ntop(AF_INET6, &a6->sin6_addr, buf, sizeof(buf));
        ep.address = buf;
        ep.port = ntohs(a6->sin6_port);
        static const std::string mapped = "::ffff:";
        if (ep.address.rfind(mapped, 0) == 0)
            ep.address = ep.address.substr(mapped.size());
    } else {
        const auto* a4 = reinterpret_cast<const sockaddr_in*>(&ss);
        ::inet_ntop(AF_INET, &a4->sin_addr, buf, sizeof(buf));
        ep.address = buf;
        ep.port = ntohs(a4->sin_port);
    }
    return ep;
}

Result<void, SocketError> UdpSocket::open() {
    return open(AF_INET);
}

Result<void, SocketError> UdpSocket::open(int family) {
    if (isOpen()) return std::unexpected(SocketError::AlreadyConnected);

    m_fd = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (m_fd == INVALID_SOCKET)
        return std::unexpected(mapErrno(errno));
    m_family = family;

    if (!setNonBlocking(m_fd)) {
        close();
        return std::unexpected(SocketError::SystemError);
    }
    return {};
}

Result<void, SocketError> UdpSocket::bind(const std::string& address, uint16_t port) {
    if (isOpen()) return std::unexpected(SocketError::AlreadyConnected);

    sockaddr_storage addr{};
    socklen_t addrLen = 0;
    int family = AF_INET;
    bool dualStack = false;

    if (address == "::" || address == "::0") {
        family = AF_INET6;
        dualStack = true;
    } else if (!address.empty() && address.find(':') != std::string::npos) {
        family = AF_INET6;
    }

    auto res = open(family);
    if (!res) return res;

    // Reuse address so quick server restarts do not fail
    int opt = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    if (family == AF_INET6) {
        if (dualStack) {
            // Accept IPv4 clients too (they arrive as v4-mapped addresses)
            int off = 0;
            ::setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                         reinterpret_cast<const char*>(&off), sizeof(off));
        }
        UdpEndpoint ep{address.empty() ? std::string("::") : address, port};
        addrLen = parseEndpoint(ep, addr);
    } else {
        UdpEndpoint ep{address.empty() ? std::string("0.0.0.0") : address, port};
        addrLen = parseEndpoint(ep, addr);
    }
    if (addrLen == 0) {
        close();
        return std::unexpected(SocketError::InvalidAddress);
    }

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), addrLen) == SOCKET_ERROR) {
        SocketError err = mapErrno(errno);
        close();
        return std::unexpected(err);
    }

    // Port 0 = ephemeral; read back the assigned port
    sockaddr_storage bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&bound), &len) == 0)
        m_localPort = endpointFromSockAddr(bound).port;
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
    if (m_localPort != 0) return m_localPort;
    // An unbound client socket gets an ephemeral port on first send;
    // read it back from the OS
    if (!isOpen()) return 0;
    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&ss), &len) == 0)
        return endpointFromSockAddr(ss).port;
    return 0;
}

// ============================================================================
// I/O
// ============================================================================

Result<size_t, SocketError>
UdpSocket::sendTo(std::span<const uint8_t> data, const UdpEndpoint& to) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);

    sockaddr_storage addr{};
    socklen_t addrLen = parseEndpoint(to, addr);
    if (addrLen == 0)
        return std::unexpected(SocketError::InvalidAddress);
    // Dual-stack (IPv6) sockets can also reach IPv4 targets via v4-mapped form
    if (addr.ss_family != m_family && m_family == AF_INET6) {
        auto* a4 = reinterpret_cast<sockaddr_in*>(&addr);
        sockaddr_in6 mapped{};
        mapped.sin6_family = AF_INET6;
        mapped.sin6_port = a4->sin_port;
        std::memset(&mapped.sin6_addr, 0, 10);
        reinterpret_cast<uint8_t*>(&mapped.sin6_addr)[10] = 0xFF;
        reinterpret_cast<uint8_t*>(&mapped.sin6_addr)[11] = 0xFF;
        std::memcpy(reinterpret_cast<uint8_t*>(&mapped.sin6_addr) + 12,
                    &a4->sin_addr, 4);
        std::memcpy(&addr, &mapped, sizeof(mapped));
        addrLen = sizeof(mapped);
    }

    ssize_t sent = ::sendto(m_fd,
                            reinterpret_cast<const char*>(data.data()),
                            data.size(), 0,
                            reinterpret_cast<sockaddr*>(&addr), addrLen);
    if (sent == SOCKET_ERROR)
        return std::unexpected(mapErrno(errno));
    return static_cast<size_t>(sent);
}

Result<UdpDatagram, SocketError> UdpSocket::recvFrom(size_t maxSize) {
    if (!isOpen()) return std::unexpected(SocketError::NotConnected);

    UdpDatagram dg;
    dg.data.resize(maxSize);

    sockaddr_storage from{};
    socklen_t fromLen = sizeof(from);
    ssize_t n = ::recvfrom(m_fd, reinterpret_cast<char*>(dg.data.data()),
                           maxSize, 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n == SOCKET_ERROR) {
        dg.data.clear();
        return std::unexpected(mapErrno(errno));
    }

    dg.data.resize(static_cast<size_t>(n));
    dg.from = endpointFromSockAddr(from);
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
    hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0 || !result)
        return std::unexpected(SocketError::InvalidAddress);

    UdpEndpoint ep;
    for (auto* rp = result; rp; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET && rp->ai_family != AF_INET6) continue;
        sockaddr_storage ss{};
        std::memcpy(&ss, rp->ai_addr, rp->ai_addrlen);
        ep = endpointFromSockAddr(ss);
        ep.port = port;
        break;
    }
    ::freeaddrinfo(result);
    if (ep.address.empty())
        return std::unexpected(SocketError::InvalidAddress);
    return ep;
}

} // namespace OmniNet
