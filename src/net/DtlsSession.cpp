// OmniNet - Pump-driven DTLS peer over memory BIOs
#include "net/DtlsSession.h"

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

} // namespace

DtlsSession::~DtlsSession() {
    if (m_ssl) SSL_free(m_ssl);  // frees both BIOs
    if (m_ctx) SSL_CTX_free(m_ctx);
}

// ============================================================================
// Setup
// ============================================================================

Result<void, SocketError> DtlsSession::makeSsl() {
    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) return std::unexpected(sslErr("SSL_new"));
    SSL_set_app_data(m_ssl, this);

    // Datagram-preserving memory BIOs (OpenSSL 3): each BIO_read returns one
    // full datagram, so encrypted records forward 1:1 over UDP
    m_rbio = BIO_new(BIO_s_dgram_mem());
    m_wbio = BIO_new(BIO_s_dgram_mem());
    if (!m_rbio || !m_wbio) return std::unexpected(sslErr("BIO_new"));
    SSL_set_bio(m_ssl, m_rbio, m_wbio);  // m_ssl takes ownership of both

    // Keep handshake flights inside a UDP datagram
    DTLS_set_link_mtu(m_ssl, 1200);
    return {};
}

Result<void, SocketError> DtlsSession::initServer(const std::string& certFile,
                                                  const std::string& keyFile,
                                                  bool requireCookie) {
    m_isServer = true;
    m_requireCookie = requireCookie;

    m_ctx = SSL_CTX_new(DTLS_server_method());
    if (!m_ctx) return std::unexpected(sslErr("SSL_CTX_new"));
    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, nullptr);

    if (SSL_CTX_use_certificate_file(m_ctx, certFile.c_str(), SSL_FILETYPE_PEM) != 1)
        return std::unexpected(sslErr("certificate"));
    if (SSL_CTX_use_PrivateKey_file(m_ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1)
        return std::unexpected(sslErr("private key"));

    if (requireCookie) {
        SSL_CTX_set_options(m_ctx, SSL_OP_COOKIE_EXCHANGE);
        SSL_CTX_set_cookie_generate_cb(m_ctx, cookieGenerateCb);
        SSL_CTX_set_cookie_verify_cb(m_ctx, cookieVerifyCb);
        ensureCookieSecret();
    }
    return makeSsl();
}

Result<void, SocketError> DtlsSession::initClient(bool verifyPeer) {
    m_isServer = false;
    m_ctx = SSL_CTX_new(DTLS_client_method());
    if (!m_ctx) return std::unexpected(sslErr("SSL_CTX_new"));
    SSL_CTX_set_verify(m_ctx, verifyPeer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
    return makeSsl();
}

void DtlsSession::setPeerAddress(const sockaddr_in& addr) {
    m_peerAddr = addr;
}

// ============================================================================
// Cookie exchange (stateless; HMAC over packed addr||port — no struct padding)
// ============================================================================

void DtlsSession::ensureCookieSecret() {
    if (m_cookieSecret.empty()) {
        m_cookieSecret.resize(32);
        RAND_bytes(m_cookieSecret.data(), static_cast<int>(m_cookieSecret.size()));
    }
}

unsigned int DtlsSession::makeCookie(unsigned char* out, unsigned int maxLen) const {
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

bool DtlsSession::checkCookie(const unsigned char* cookie, unsigned int len) const {
    if (len == 0 || len > 32) return false;
    unsigned char expected[EVP_MAX_MD_SIZE];
    unsigned int n = makeCookie(expected, sizeof(expected));
    return n == len && CRYPTO_memcmp(cookie, expected, n) == 0;
}

int DtlsSession::cookieGenerateCb(SSL* ssl, unsigned char* cookie, unsigned int* len) {
    auto* self = static_cast<DtlsSession*>(SSL_get_app_data(ssl));
    if (!self) return 0;
    *len = self->makeCookie(cookie, 255);
    return 1;
}

int DtlsSession::cookieVerifyCb(SSL* ssl, const unsigned char* cookie, unsigned int len) {
    auto* self = static_cast<DtlsSession*>(SSL_get_app_data(ssl));
    if (!self) return 0;
    return self->checkCookie(cookie, len) ? 1 : 0;
}

// ============================================================================
// Transport-facing
// ============================================================================

void DtlsSession::inputDatagram(std::span<const uint8_t> datagram) {
    if (!m_rbio || datagram.empty()) return;
    BIO_write(m_rbio, datagram.data(), static_cast<int>(datagram.size()));
}

void DtlsSession::pollOutgoing(std::vector<std::vector<uint8_t>>& out) {
    if (!m_wbio) return;
    uint8_t buf[65536];
    int n;
    while ((n = BIO_read(m_wbio, buf, sizeof(buf))) > 0)
        out.emplace_back(buf, buf + n);
}

// ============================================================================
// Handshake
// ============================================================================

void DtlsSession::drive() {
    if (!m_ssl || m_established) return;

    if (m_isServer && m_requireCookie && !m_cookieVerified) {
        // Stateless phase: answer ClientHello with HelloVerifyRequest until
        // the client returns a valid cookie. DTLSv1_listen dereferences the
        // client-address out-param unconditionally — it may not be nullptr.
        BIO_ADDR* clientAddr = BIO_ADDR_new();
        int rc = DTLSv1_listen(m_ssl, clientAddr);
        BIO_ADDR_free(clientAddr);
        if (rc > 0) m_cookieVerified = true;
        return;  // outgoing (HelloVerifyRequest) drained via pollOutgoing
    }

    int rc = m_isServer ? SSL_accept(m_ssl) : SSL_connect(m_ssl);
    if (rc == 1) {
        m_established = true;
        return;
    }
    int err = SSL_get_error(m_ssl, rc);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
        ERR_clear_error();  // handshake alert etc.; caller times out
}

int DtlsSession::nextTimeoutMs() const {
    if (!m_ssl || m_established) return -1;
    timeval tv{};
    if (!DTLSv1_get_timeout(m_ssl, &tv)) return -1;
    auto ms = static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
    return ms > 0 ? static_cast<int>(ms) : 0;
}

void DtlsSession::handleTimeout() {
    if (m_ssl && !m_established)
        DTLSv1_handle_timeout(m_ssl);
}

// ============================================================================
// Application-facing
// ============================================================================

Result<void, SocketError> DtlsSession::encrypt(std::span<const uint8_t> plaintext) {
    if (!m_established) return std::unexpected(SocketError::NotConnected);
    int rc = SSL_write(m_ssl, plaintext.data(), static_cast<int>(plaintext.size()));
    if (rc <= 0) return std::unexpected(sslErr("SSL_write"));
    return {};
}

void DtlsSession::pollPlaintext(std::vector<std::vector<uint8_t>>& out) {
    if (!m_ssl || !m_established) return;
    uint8_t buf[65536];
    for (;;) {
        int n = SSL_read(m_ssl, buf, sizeof(buf));
        if (n <= 0) {
            int err = SSL_get_error(m_ssl, n);
            if (err == SSL_ERROR_WANT_READ) break;
            if (err == SSL_ERROR_ZERO_RETURN) break;  // close_notify
            ERR_clear_error();
            break;
        }
        out.emplace_back(buf, buf + n);
    }
}

std::string DtlsSession::cipherName() const {
    if (!m_ssl) return {};
    const char* name = SSL_get_cipher_name(m_ssl);
    return name ? name : "";
}

} // namespace OmniNet
