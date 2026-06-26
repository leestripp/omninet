#include "TLSSocket.h"
#include "utils/Utils.h"
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <iostream>

namespace OmniNet {

// ============================================================================
// TLSSocket
// ============================================================================

TLSSocket::TLSSocket() : m_serverMode(false), m_handshakeDone(false) {
    initContext(TLSMode::CLIENT);
}

TLSSocket::TLSSocket(std::unique_ptr<Socket> socket) 
    : Socket(socket->fd()), m_serverMode(true), m_handshakeDone(false), m_underlyingSocket(std::move(socket)) {
    initContext(TLSMode::SERVER);
}

TLSSocket::TLSSocket(SOCKET fd, bool serverMode)
    : Socket(fd), m_serverMode(serverMode), m_handshakeDone(false) {
    initContext(serverMode ? TLSMode::SERVER : TLSMode::CLIENT);
}

// v0.7.2 (codemax Bug 1): constructor that wraps an unconnected
// socket fd with a shared TLSContext. Used by TLSContext::createSocket().
TLSSocket::TLSSocket(std::shared_ptr<TLSContext> context)
    : Socket(INVALID_SOCKET), m_serverMode(!context || context->serverMode()),
      m_handshakeDone(false), m_context(std::move(context)) {
    if (m_context) {
        // Use the shared context's SSL_CTX instead of creating our own.
        // Note: initContext() is NOT called here because m_ctx is set
        // by setContext() below.
        setContext(m_context);
    } else {
        initContext(TLSMode::CLIENT);
    }
}

TLSSocket::~TLSSocket() {
    close();
    cleanupContext();
}

// v0.7.2 (codemax Bug 1): adopt a shared TLSContext. Frees our
// own m_ctx (if any) and uses the shared context's SSL_CTX.
void TLSSocket::setContext(std::shared_ptr<TLSContext> context) {
    m_context = std::move(context);
    if (m_context) {
        cleanupContext();   // free our own m_ctx if any
        m_ctx = m_context->context();
        m_serverMode = m_context->serverMode();
    }
}

void TLSSocket::setCertFile(const std::string& path) {
    m_certFile = path;
    if (m_ctx) {
        SSL_CTX_use_certificate_file(m_ctx, path.c_str(), SSL_FILETYPE_PEM);
    }
}

void TLSSocket::setKeyFile(const std::string& path) {
    m_keyFile = path;
    if (m_ctx) {
        SSL_CTX_use_PrivateKey_file(m_ctx, path.c_str(), SSL_FILETYPE_PEM);
    }
}

void TLSSocket::setCaFile(const std::string& path) {
    m_caFile = path;
    if (m_ctx) {
        SSL_CTX_load_verify_locations(m_ctx, path.c_str(), nullptr);
    }
}

void TLSSocket::setCaDir(const std::string& path) {
    m_caDir = path;
    if (m_ctx) {
        SSL_CTX_load_verify_locations(m_ctx, nullptr, path.c_str());
    }
}

void TLSSocket::setVerifyMode(TLSVerify mode) {
    if (!m_ctx) return;
    
    switch (mode) {
        case TLSVerify::NONE:
            SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, nullptr);
            break;
        case TLSVerify::PEER:
            SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER, nullptr);
            break;
        case TLSVerify::PEER_STRICT:
            SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
            break;
        case TLSVerify::CLIENT_ONCE:
            SSL_CTX_set_verify(m_ctx, SSL_VERIFY_CLIENT_ONCE, nullptr);
            break;
    }
}

void TLSSocket::setMinVersion(int major, int minor) {
    if (!m_ctx) return;
    
    int version = TLS1_2_VERSION;
    if (major == 1 && minor == 1) version = TLS1_1_VERSION;
    else if (major == 1 && minor == 2) version = TLS1_2_VERSION;
    
    SSL_CTX_set_min_proto_version(m_ctx, version);
}

void TLSSocket::setMaxVersion(int major, int minor) {
    if (!m_ctx) return;
    
    int version = TLS1_3_VERSION;
    if (major == 1 && minor == 2) version = TLS1_2_VERSION;
    else if (major == 1 && minor == 1) version = TLS1_1_VERSION;
    
    SSL_CTX_set_max_proto_version(m_ctx, version);
}

void TLSSocket::setSNIHostname(const std::string& hostname) {
    m_sniHostname = hostname;
}

void TLSSocket::setALPNProtocols(const std::vector<std::string>& protocols) {
    if (protocols.empty()) return;
    if (!m_ctx) return;

    // v0.7.2 (codemax Bug 2): build the ALPN wire-format list
    // (RFC 7301): for each protocol, 1 byte length followed by
    // the protocol name bytes.
    std::vector<unsigned char> wire;
    for (const auto& p : protocols) {
        if (p.empty() || p.size() > 255) continue;  // ALPN length is 1 byte
        wire.push_back(static_cast<unsigned char>(p.size()));
        wire.insert(wire.end(), p.begin(), p.end());
    }
    if (wire.empty()) return;

    // Apply to the SSL_CTX (used by the next SSL_new).
    // Returns 0 on success, non-zero on error.
    SSL_CTX_set_alpn_protos(m_ctx, wire.data(), static_cast<unsigned int>(wire.size()));
}

// ============================================================================
// Connection
// ============================================================================

void TLSSocket::connect(const std::string& host, uint16_t port) {
    Socket::connect(host, port);
    doHandshake();
}

void TLSSocket::performHandshake() {
    // Keep trying until handshake completes or fails
    int iterations = 0;
    const int maxIterations = 100; // 100 * 10ms = 1 second timeout
    
    while (!m_handshakeDone && iterations < maxIterations) {
        iterations++;
        
        // Use select() to wait for socket to be ready
        fd_set readFds, writeFds;
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_SET(m_fd, &readFds);
        FD_SET(m_fd, &writeFds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms timeout
        
        int selectResult = select(m_fd + 1, &readFds, &writeFds, nullptr, &tv);
        if (selectResult < 0) {
            throw TLSException(std::string("select() failed during TLS handshake: ") + strerror(errno), errno);
        }
        
        // Now try the handshake
        doHandshake();
    }
    
    if (!m_handshakeDone) {
        throw TLSException(std::string("TLS handshake timed out"), -1);
    }
}

bool TLSSocket::shutdown() {
    if (m_ssl) {
        SSL_shutdown(m_ssl);
    }
    return true;
}

// ============================================================================
// I/O
// ============================================================================

size_t TLSSocket::send(std::span<const uint8_t> data) {
    if (!m_ssl) return 0;
    
    int ret = SSL_write(m_ssl, data.data(), static_cast<int>(data.size()));
    if (ret > 0) return static_cast<size_t>(ret);
    return 0;
}

size_t TLSSocket::send(std::string_view data) {
    return send(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

void TLSSocket::sendAll(std::span<const uint8_t> data) {
    size_t sent = 0;
    while (sent < data.size()) {
        size_t bytes = send(data.subspan(sent));
        if (bytes == 0) break;
        sent += bytes;
    }
}

size_t TLSSocket::recv(std::span<uint8_t> buffer) {
    if (!m_ssl) return 0;
    
    int ret = SSL_read(m_ssl, buffer.data(), static_cast<int>(buffer.size()));
    if (ret > 0) {
        return static_cast<size_t>(ret);
    }
    
    if (ret <= 0) {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Would block, caller should retry when socket is readable
            return 0;
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            // Connection closed gracefully
            return 0;
        }
        // Other error
        return 0;
    }
    
    return 0;
}

std::vector<uint8_t> TLSSocket::recvAll(size_t maxSize) {
    std::vector<uint8_t> result;
    result.reserve(4096);
    
    std::vector<uint8_t> buffer(4096);
    int iterations = 0;
    const int maxIterations = 200; // 200 * 10ms = 2 second timeout
    
    while (result.size() < maxSize && iterations < maxIterations) {
        iterations++;
        
        // Check if there's pending SSL data first
        if (m_ssl) {
            int pending = SSL_pending(m_ssl);
            if (pending > 0) {
                size_t bytes = recv(buffer);
                if (bytes > 0) {
                    result.insert(result.end(), buffer.begin(), buffer.begin() + bytes);
                    continue;
                }
            }
        }
        
        // Use select to wait for data
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(m_fd, &readFds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms
        
        int selectResult = select(m_fd + 1, &readFds, nullptr, nullptr, &tv);
        if (selectResult <= 0) {
            if (result.empty()) {
                continue; // No data yet, keep waiting
            }
            break; // We have some data, return it
        }
        
        size_t bytes = recv(buffer);
        if (bytes == 0) {
            break;
        }
        result.insert(result.end(), buffer.begin(), buffer.begin() + bytes);
    }
    
    return result;
}

std::vector<uint8_t> TLSSocket::recvExactly(size_t n) {
    std::vector<uint8_t> result(n);
    size_t pos = 0;

    while (pos < n) {
        size_t bytes = recv(std::span<uint8_t>(result.data() + pos, n - pos));
        if (bytes == 0) {
            result.resize(pos);
            return result;
        }
        pos += bytes;
    }

    return result;
}

// v0.7.2 (codemax Bug 3): override readLine() to use the overridden
// recv() (which goes through SSL_read) instead of the base class's
// raw ::recv(). Without this, calling readLine() on a TLSSocket
// returned raw ciphertext (the TLS record header), causing
// "malformed status line" errors on any HTTPS request.
std::optional<std::string> TLSSocket::readLine(size_t maxLength) {
    std::string line;
    line.reserve(256);

    while (line.size() < maxLength) {
        uint8_t byte = 0;
        std::span<uint8_t> buf(&byte, 1);
        size_t n = recv(buf);
        if (n == 0) {
            // Connection closed
            if (line.empty()) return std::nullopt;
            return line;
        }
        if (byte == '\n') {
            // Strip trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        line.push_back(static_cast<char>(byte));
    }

    // Hit maxLength without finding \n — return what we have.
    return line;
}

// ============================================================================
// State
// ============================================================================

bool TLSSocket::isSecure() const {
    return m_ssl != nullptr && m_handshakeDone;
}

std::string TLSSocket::getCipher() {
    if (!m_ssl) return "";
    return SSL_get_cipher(m_ssl);
}

std::string TLSSocket::getVersion() {
    if (!m_ssl) return "";
    return SSL_get_version(m_ssl);
}

bool TLSSocket::hasPeerCertificate() {
    if (!m_ssl) return false;
    return SSL_get_peer_certificate(m_ssl) != nullptr;
}

std::optional<std::string> TLSSocket::getPeerCertificateCN() {
    if (!m_ssl) return std::nullopt;
    
    X509* cert = SSL_get_peer_certificate(m_ssl);
    if (!cert) return std::nullopt;
    
    char* name = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    std::string result(name ? name : "");
    OPENSSL_free(name);
    X509_free(cert);
    
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<std::string> TLSSocket::getPeerCertificateFingerprint() {
    if (!m_ssl) return std::nullopt;
    
    X509* cert = SSL_get_peer_certificate(m_ssl);
    if (!cert) return std::nullopt;
    
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    
    if (X509_digest(cert, EVP_sha256(), md, &len)) {
        std::string result;
        for (unsigned int i = 0; i < len; i++) {
            result += "0123456789ABCDEF"[(md[i] >> 4) & 0x0F];
            result += "0123456789ABCDEF"[md[i] & 0x0F];
        }
        X509_free(cert);
        return result;
    }
    
    X509_free(cert);
    return std::nullopt;
}

std::optional<std::string> TLSSocket::getALPNProtocol() {
    if (!m_ssl) return std::nullopt;
    
    const unsigned char* proto;
    unsigned int protoLen;
    SSL_get0_alpn_selected(m_ssl, &proto, &protoLen);
    
    if (!proto) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(proto), protoLen);
}

SSL* TLSSocket::ssl() const {
    return m_ssl;
}

SSL_CTX* TLSSocket::context() const {
    return m_ctx;
}

// ============================================================================
// Private Methods
// ============================================================================

void TLSSocket::initContext(TLSMode mode) {
    m_serverMode = (mode == TLSMode::SERVER);

    m_ctx = m_serverMode
        ? SSL_CTX_new(TLS_server_method())
        : SSL_CTX_new(TLS_client_method());

    if (!m_ctx) {
        throw TLSException(std::string("Failed to create SSL context"), -1);
    }

    // v0.7.2 (codemax Bug 4): for client contexts, load the system
    // CA bundle and set SSL_VERIFY_PEER by default. Without this:
    //   - SSL_VERIFY_NONE is OpenSSL's default, so any certificate
    //     (including self-signed/forged) is silently accepted
    //   - setVerifyMode(PEER) on its own doesn't load the CA store,
    //     so OpenSSL has nothing to verify against - the handshake
    //     succeeds against a forged certificate
    // Callers can still override via setVerifyMode(NONE) for
    // dev/testing, but the secure default is PEER + system CA bundle.
    if (!m_serverMode) {
        if (SSL_CTX_set_default_verify_paths(m_ctx) != 1) {
            // Non-fatal: system CA bundle may not exist on minimal
            // systems. Callers can use setCaFile() to install one.
            // We don't throw here because some users intentionally
            // use self-signed certs and configure their own CA file.
        }
        SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER, nullptr);
    }
}


void TLSSocket::cleanupContext() {
    if (m_ssl) {
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

void TLSSocket::doHandshake() {
    if (m_handshakeDone) return;

    if (!m_ssl && m_ctx) {
        m_ssl = SSL_new(m_ctx);
        if (!m_ssl) {
            throw TLSException(std::string("Failed to create SSL object"), -1);
        }
        SSL_set_fd(m_ssl, m_fd);

        if (!m_sniHostname.empty()) {
            SSL_set_tlsext_host_name(m_ssl, m_sniHostname.c_str());
        }
    }

    int ret;
    if (m_serverMode) {
        ret = SSL_accept(m_ssl);
    } else {
        ret = SSL_connect(m_ssl);
    }

    if (ret != 1) {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return;
        }
        // Log all pending OpenSSL errors
        std::cerr << "[TLSSocket] OpenSSL error queue:" << std::endl;
        unsigned long e;
        while ((e = ERR_get_error()) != 0) {
            char buf[256];
            ERR_error_string_n(e, buf, sizeof(buf));
            std::cerr << "  - " << buf << " (lib=" << ERR_lib_error_string(e)
                      << ", reason=" << ERR_reason_error_string(e) << ")" << std::endl;
        }
        throw TLSException(std::string("TLS handshake failed: ") + Crypto::lastError(), err);
    }

    m_handshakeDone = true;
}

// v0.6.0: Init SSL* if needed (no handshake step)
void TLSSocket::initHandshake() {
    if (m_handshakeDone || m_ssl) return;

    if (!m_ctx) {
        throw TLSException(std::string("SSL_CTX not initialized"), -1);
    }

    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) {
        throw TLSException(std::string("Failed to create SSL object"), -1);
    }
    SSL_set_fd(m_ssl, m_fd);

    if (!m_sniHostname.empty()) {
        SSL_set_tlsext_host_name(m_ssl, m_sniHostname.c_str());
    }
}

// v0.6.0: Perform one handshake step, return WANT state
TLSSocket::HandshakeState TLSSocket::stepHandshake() {
    if (m_handshakeDone) return HandshakeState::Done;
    if (!m_ssl) {
        throw TLSException(std::string("SSL not initialized; call initHandshake first"), -1);
    }

    int ret = m_serverMode ? SSL_accept(m_ssl) : SSL_connect(m_ssl);

    if (ret == 1) {
        m_handshakeDone = true;
        return HandshakeState::Done;
    }

    int err = SSL_get_error(m_ssl, ret);
    if (err == SSL_ERROR_WANT_READ) return HandshakeState::WantRead;
    if (err == SSL_ERROR_WANT_WRITE) return HandshakeState::WantWrite;

    // Real error - log OpenSSL error queue
    std::cerr << "[TLSSocket] OpenSSL error queue:" << std::endl;
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        std::cerr << "  - " << buf << " (lib=" << ERR_lib_error_string(e)
                  << ", reason=" << ERR_reason_error_string(e) << ")" << std::endl;
    }
    return HandshakeState::Error;
}

size_t TLSSocket::sslRecv(std::span<uint8_t> buffer) {
    return recv(buffer);
}

size_t TLSSocket::sslSend(std::span<const uint8_t> data) {
    return send(data);
}

// ============================================================================
// TLSContext
// ============================================================================

TLSContext::TLSContext(SSL_CTX* ctx, bool serverMode)
    : m_ctx(ctx), m_serverMode(serverMode) {}

TLSContext::~TLSContext() {
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

SSL_CTX* TLSContext::context() const { return m_ctx; }
bool TLSContext::serverMode() const { return m_serverMode; }

// v0.7.2 (codemax Bug 1 + 5): create a server context with the
// given certificate and key files. Returns a shared_ptr so the
// context can be shared across multiple sockets.
std::shared_ptr<TLSContext> TLSContext::createServer(
    const std::string& certFile,
    const std::string& keyFile
) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        throw TLSException(std::string("Failed to create server SSL_CTX"), -1);
    }
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        throw TLSException(std::string("Failed to load server certificate: ") + certFile, -1);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        throw TLSException(std::string("Failed to load server private key: ") + keyFile, -1);
    }
    return std::shared_ptr<TLSContext>(new TLSContext(ctx, /*serverMode=*/true));
}

// v0.7.2 (codemax Bug 1): create a client context. Loads the
// system CA bundle and sets SSL_VERIFY_PEER by default so users
// don't silently accept forged certificates.
std::shared_ptr<TLSContext> TLSContext::createClient() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw TLSException(std::string("Failed to create client SSL_CTX"), -1);
    }
    // Load system CA bundle (non-fatal if it fails; callers can
    // use setCaFile() to install a custom one).
    SSL_CTX_set_default_verify_paths(ctx);
    // Default to verifying peer certificates.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    return std::shared_ptr<TLSContext>(new TLSContext(ctx, /*serverMode=*/false));
}

// v0.7.2 (codemax Bug 1 + 5): create a new TLSSocket that uses
// this shared context. Uses shared_from_this() because TLSContext
// inherits std::enable_shared_from_this<TLSContext>.
std::unique_ptr<TLSSocket> TLSContext::createSocket() {
    auto sock = std::make_unique<TLSSocket>(shared_from_this());
    return sock;
}

} // namespace OmniNet
