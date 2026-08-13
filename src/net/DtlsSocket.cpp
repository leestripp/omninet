// OmniNet - DTLS datagram socket (encrypted UDP) for game networking
//
// Fully nonblocking: the fd is set O_NONBLOCK at creation and all handshake
// loops poll + honor the DTLS retransmission timer (DTLSv1_get_timeout /
// DTLSv1_handle_timeout), so deadlines are always enforced and lost
// handshake flights are retransmitted.
#include "net/DtlsSocket.h"

#include <cerrno>
#include <chrono>
#include <cstring>

#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace OmniNet {

namespace {

SocketError sslErr(const char* what) {
    (void)what;
    ERR_clear_error();
    return SocketError::SystemError;
}

sockaddr_in toSockAddr(const UdpEndpoint& ep) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ep.port);
    ::inet_pton(AF_INET, ep.address.c_str(), &addr.sin_addr);
    return addr;
}

BIO_ADDR* toBioAddr(const sockaddr_in& from) {
    BIO_ADDR* a = BIO_ADDR_new();
    // BIO_ADDR_rawmake expects the port in network byte order
    BIO_ADDR_rawmake(a, AF_INET, &from.sin_addr, sizeof(from.sin_addr), from.sin_port);
    return a;
}

int64_t msRemaining(SteadyDeadline deadline) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
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

    // Nonblocking: handshake loops poll + DTLS timers drive retransmission
#ifdef _WIN32
    u_long mode = 1;
    ::ioctlsocket(m_fd, FIONBIO, &mode);
#else
    int flags = ::fcntl(m_fd, F_GETFL, 0);
    ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
#endif
    return {};
}

void DtlsSocket::setBlocking(bool blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    ::ioctlsocket(m_fd, FIONBIO, &mode);
#else
    int flags = ::fcntl(m_fd, F_GETFL, 0);
    ::fcntl(m_fd, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
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
// Handshake driver (shared by server accept and client connect)
// ============================================================================

Result<void, SocketError> DtlsSocket::driveHandshake(bool server,
                                                     SteadyDeadline deadline) {
    for (;;) {
        int rc = server ? SSL_accept(m_ssl) : SSL_connect(m_ssl);
        if (rc == 1) { m_established = true; return {}; }

        int err = SSL_get_error(m_ssl, rc);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            return std::unexpected(sslErr(server ? "SSL_accept" : "SSL_connect"));

        int64_t remain = msRemaining(deadline);
        if (remain <= 0) return std::unexpected(SocketError::Timeout);

        // Wait at most until the DTLS retransmission timer fires
        int waitMs = static_cast<int>(remain);
        timeval tv{};
        if (DTLSv1_get_timeout(m_ssl, &tv)) {
            int64_t dtlsMs = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            if (dtlsMs <= 0) {
                DTLSv1_handle_timeout(m_ssl);  // retransmit flight now
                continue;
            }
            if (dtlsMs < waitMs) waitMs = static_cast<int>(dtlsMs);
        }

        if (!waitReadable(waitMs)) {
            // Woke for the timer (or spuriously): let DTLS retransmit if due
            DTLSv1_handle_timeout(m_ssl);
        }
    }
}

// ============================================================================
// Cookie exchange (stateless DoS hardening)
// ============================================================================

void DtlsSocket::ensureCookieSecret() {
    if (m_cookieSecret.empty()) {
        m_cookieSecret.resize(32);
        RAND_bytes(m_cookieSecret.data(), static_cast<int>(m_cookieSecret.size()));
    }
}

// Cookie = HMAC-SHA256(secret, client IPv4 addr || port), truncated to 32 B.
// Stateless: no per-connection memory until the client proves its address.
unsigned int DtlsSocket::makeCookie(unsigned char* out, unsigned int maxLen) const {
    // Packed 6-byte blob: addr || port (no struct padding — the HMAC input
    // must be byte-identical between generate and verify)
    unsigned char blob[6];
    std::memcpy(blob, &m_peerAddr.sin_addr, 4);
    std::memcpy(blob + 4, &m_peerAddr.sin_port, 2);
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int macLen = 0;
    HMAC(EVP_sha256(), m_cookieSecret.data(), static_cast<int>(m_cookieSecret.size()),
         blob, sizeof(blob), mac, &macLen);
    unsigned int n = macLen < maxLen ? macLen : maxLen;
    if (n > 32) n = 32;
    std::memcpy(out, mac, n);
    return n;
}

bool DtlsSocket::checkCookie(const unsigned char* cookie, unsigned int len) const {
    if (len == 0 || len > 32) return false;
    unsigned char expected[EVP_MAX_MD_SIZE];
    unsigned int n = makeCookie(expected, sizeof(expected));
    return n == len && CRYPTO_memcmp(cookie, expected, n) == 0;
}

int DtlsSocket::cookieGenerateCb(SSL* ssl, unsigned char* cookie, unsigned int* len) {
    auto* self = static_cast<DtlsSocket*>(SSL_get_app_data(ssl));
    if (!self) return 0;
    *len = self->makeCookie(cookie, 255 /* DTLS cookie max */);
    return 1;
}

int DtlsSocket::cookieVerifyCb(SSL* ssl, const unsigned char* cookie, unsigned int len) {
    auto* self = static_cast<DtlsSocket*>(SSL_get_app_data(ssl));
    if (!self) return 0;
    return self->checkCookie(cookie, len) ? 1 : 0;
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
                                             int timeoutMs,
                                             bool requireCookie) {
    if (m_fd < 0) return std::unexpected(SocketError::NotConnected);
    if (!waitReadable(timeoutMs)) return std::unexpected(SocketError::Timeout);

    // Learn the client address from the first datagram without consuming it
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    uint8_t peekBuf[1];
    ssize_t n = ::recvfrom(m_fd, peekBuf, sizeof(peekBuf), MSG_PEEK,
                           reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n < 0) return std::unexpected(SocketError::SystemError);

    m_peerAddr = from;
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

    if (requireCookie)
        SSL_CTX_set_options(m_ctx, SSL_OP_COOKIE_EXCHANGE);

    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) return std::unexpected(sslErr("SSL_new"));
    SSL_set_app_data(m_ssl, this);

    BIO* bio = BIO_new_dgram(m_fd, BIO_NOCLOSE);
    SSL_set_bio(m_ssl, bio, bio);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    if (requireCookie) {
        // Address verification BEFORE committing handshake state: the first
        // ClientHello is answered with a stateless HelloVerifyRequest; only
        // a client that returns the cookie (proving it owns the address)
        // proceeds to a real handshake.
        SSL_CTX_set_cookie_generate_cb(m_ctx, cookieGenerateCb);
        SSL_CTX_set_cookie_verify_cb(m_ctx, cookieVerifyCb);
        ensureCookieSecret();

        BIO_ADDR* peerBioAddr = toBioAddr(from);
        BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_PEER, 0, peerBioAddr);
        BIO_ADDR_free(peerBioAddr);

        // DTLSv1_listen expects a blocking socket (OpenSSL maps a would-block
        // dgram read to SSL_ERROR_SYSCALL, not WANT_READ — s_server does the
        // same). Bound each wait with SO_RCVTIMEO slices so the overall
        // deadline is still honored.
        setBlocking(true);
        timeval rto{};
        rto.tv_usec = 200 * 1000;  // 200ms slices
        ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &rto, sizeof(rto));

        BIO_ADDR* clientAddr = BIO_ADDR_new();
        bool verified = false;
        while (!verified) {
            if (msRemaining(deadline) <= 0) break;

            errno = 0;
            int rc = DTLSv1_listen(m_ssl, clientAddr);
            if (rc > 0) { verified = true; break; }  // cookie verified

            int err = SSL_get_error(m_ssl, rc);
            // Receive-timeout slice elapsed: loop and re-check the deadline
            if (err == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                continue;
            break;  // hard error
        }
        BIO_ADDR_free(clientAddr);

        // Restore nonblocking mode for the handshake phase
        timeval zero{};
        ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &zero, sizeof(zero));
        setBlocking(false);

        if (!verified)
            return std::unexpected(msRemaining(deadline) <= 0 ? SocketError::Timeout
                                                              : sslErr("DTLSv1_listen"));
    }

    // Connect the UDP socket so the dgram BIO talks to this client only
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&from), fromLen) != 0) {
        std::fprintf(stderr, "[dtls] accept connect() errno=%d (%s)\n", errno, std::strerror(errno));
        return std::unexpected(SocketError::SystemError);
    }
    {
        BIO_ADDR* connAddr = toBioAddr(from);
        BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, connAddr);
        BIO_ADDR_free(connAddr);
    }

    return driveHandshake(true, deadline);
}

// ============================================================================
// Client
// ============================================================================

Result<void, SocketError> DtlsSocket::connect(const std::string& host, uint16_t port,
                                              int timeoutMs, bool verifyPeer) {
    auto ep = UdpSocket::resolve(host, port);
    if (!ep) return std::unexpected(ep.error());
    if (ep->address.find(':') != std::string::npos)
        return std::unexpected(SocketError::InvalidAddress);  // DTLS is IPv4-only for now
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
    auto result = driveHandshake(false, deadline);
    if (!result) close();
    return result;
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
