// OmniNet - DTLS datagram socket
#include "net/DtlsSocket.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace OmniNet {

namespace {

SocketError sslErr(const char* what) {
    unsigned long e;
    while ((e = ERR_get_error()) != 0)
        std::fprintf(stderr, "[dtls] %s: %s\n", what, ERR_error_string(e, nullptr));
    return SocketError::SystemError;
}

sockaddr_in toSockAddr(const UdpEndpoint& ep) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ep.port);
    ::inet_pton(AF_INET, ep.address.c_str(), &addr.sin_addr);
    return addr;
}

} // namespace

DtlsSocket::~DtlsSocket() {
    close();
}

void DtlsSocket::close() {
    if (m_ssl) {
        if (m_established) SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_fd >= 0) {
#ifdef _WIN32
        ::closesocket(m_fd);
#else
        ::close(m_fd);
#endif
        m_fd = -1;
    }
    m_established = false;
}

Result<void, SocketError> DtlsSocket::openUdp() {
    m_fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_fd < 0) return std::unexpected(SocketError::SystemError);
    return {};
}

bool DtlsSocket::waitReadable(int timeoutMs) const {
    pollfd pfd{};
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    int rc;
    do { rc = ::poll(&pfd, 1, timeoutMs); } while (rc < 0 && errno == EINTR);
    return rc > 0 && (pfd.revents & POLLIN);
}

uint16_t DtlsSocket::localPort() const {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
        return ntohs(addr.sin_port);
    return 0;
}

// ============================================================================
// Server
// ============================================================================

Result<void, SocketError> DtlsSocket::bind(const std::string& address, uint16_t port) {
    auto res = openUdp();
    if (!res) return res;

    int opt = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr = toSockAddr({address.empty() ? "0.0.0.0" : address, port});
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close();
        return std::unexpected(SocketError::AddressInUse);
    }
    return {};
}

Result<void, SocketError> DtlsSocket::accept(const std::string& certFile,
                                             const std::string& keyFile,
                                             int timeoutMs) {
    if (m_fd < 0) return std::unexpected(SocketError::NotConnected);
    if (!waitReadable(timeoutMs)) return std::unexpected(SocketError::Timeout);

    // Learn the client address from the first datagram without consuming it
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    uint8_t peekBuf[1];
    ssize_t n = ::recvfrom(m_fd, peekBuf, sizeof(peekBuf), MSG_PEEK,
                           reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n < 0) return std::unexpected(SocketError::SystemError);

    char buf[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &from.sin_addr, buf, sizeof(buf));
    m_peer = {buf, ntohs(from.sin_port)};

    m_ctx = SSL_CTX_new(DTLS_server_method());
    if (!m_ctx) return std::unexpected(sslErr("SSL_CTX_new"));
    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, nullptr);  // clients rarely have certs

    if (SSL_CTX_use_certificate_file(m_ctx, certFile.c_str(), SSL_FILETYPE_PEM) != 1)
        return std::unexpected(sslErr("certificate"));
    if (SSL_CTX_use_PrivateKey_file(m_ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1)
        return std::unexpected(sslErr("private key"));

    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) return std::unexpected(sslErr("SSL_new"));
    SSL_set_app_data(m_ssl, this);

    BIO* bio = BIO_new_dgram(m_fd, BIO_NOCLOSE);
    SSL_set_bio(m_ssl, bio, bio);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Connect the UDP socket so the dgram BIO talks to this client only
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&from), fromLen) != 0)
        return std::unexpected(SocketError::SystemError);

    // Drive the handshake; DTLS retransmits internally via the BIO
    for (;;) {
        int rc = SSL_accept(m_ssl);
        if (rc == 1) { m_established = true; return {}; }
        int err = SSL_get_error(m_ssl, rc);
        if (err == SSL_ERROR_WANT_READ) {
            auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remain <= 0 || !waitReadable(static_cast<int>(remain)))
                return std::unexpected(SocketError::Timeout);
            continue;
        }
        return std::unexpected(sslErr("SSL_accept"));
    }
}

// ============================================================================
// Client
// ============================================================================

Result<void, SocketError> DtlsSocket::connect(const std::string& host, uint16_t port,
                                              int timeoutMs, bool verifyPeer) {
    auto ep = UdpSocket::resolve(host, port);
    if (!ep) return std::unexpected(ep.error());
    m_peer = *ep;

    auto res = openUdp();
    if (!res) return res;

    sockaddr_in addr = toSockAddr(m_peer);
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close();
        return std::unexpected(SocketError::SystemError);
    }

    m_ctx = SSL_CTX_new(DTLS_client_method());
    if (!m_ctx) { close(); return std::unexpected(sslErr("SSL_CTX_new")); }
    SSL_CTX_set_verify(m_ctx, verifyPeer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);

    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) { close(); return std::unexpected(sslErr("SSL_new")); }

    BIO* bio = BIO_new_dgram(m_fd, BIO_NOCLOSE);
    SSL_set_bio(m_ssl, bio, bio);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        int rc = SSL_connect(m_ssl);
        if (rc == 1) { m_established = true; return {}; }
        int err = SSL_get_error(m_ssl, rc);
        if (err == SSL_ERROR_WANT_READ) {
            auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remain <= 0 || !waitReadable(static_cast<int>(remain))) {
                close();
                return std::unexpected(SocketError::Timeout);
            }
            continue;
        }
        close();
        return std::unexpected(sslErr("SSL_connect"));
    }
}

// ============================================================================
// I/O
// ============================================================================

Result<size_t, SocketError> DtlsSocket::send(std::span<const uint8_t> data) {
    if (!m_established) return std::unexpected(SocketError::NotConnected);
    int rc = SSL_write(m_ssl, data.data(), static_cast<int>(data.size()));
    if (rc <= 0) return std::unexpected(sslErr("SSL_write"));
    return static_cast<size_t>(rc);
}

Result<std::vector<uint8_t>, SocketError> DtlsSocket::recv(size_t maxSize, int timeoutMs) {
    if (!m_established) return std::unexpected(SocketError::NotConnected);
    if (!waitReadable(timeoutMs)) return std::unexpected(SocketError::WouldBlock);

    std::vector<uint8_t> buf(maxSize);
    int rc = SSL_read(m_ssl, buf.data(), static_cast<int>(buf.size()));
    if (rc <= 0) {
        int err = SSL_get_error(m_ssl, rc);
        if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL)
            return std::unexpected(SocketError::ConnectionReset);
        return std::unexpected(sslErr("SSL_read"));
    }
    buf.resize(static_cast<size_t>(rc));
    return buf;
}

std::string DtlsSocket::cipherName() const {
    if (!m_ssl) return {};
    const char* name = SSL_get_cipher_name(m_ssl);
    return name ? name : "";
}

} // namespace OmniNet
