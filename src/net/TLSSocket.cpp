// OmniNet - TLS Socket Implementation
#include "TLSSocket.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

namespace OmniNet {

// ============================================================================
// TLS Exception
// ============================================================================

TLSException::TLSException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

// ============================================================================
// TLSSocket
// ============================================================================

TLSSocket::TLSSocket() {
    initContext(TLSMode::CLIENT);
}

TLSSocket::TLSSocket(std::unique_ptr<Socket> acceptedSocket)
    : Socket(std::move(acceptedSocket)) {
    initContext(TLSMode::SERVER);
}

TLSSocket::TLSSocket(SOCKET fd, bool serverMode)
    : Socket(fd) {
    initContext(serverMode ? TLSMode::SERVER : TLSMode::CLIENT);
}

TLSSocket::~TLSSocket() {
    close();
    cleanupContext();
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
    SSL_CTX_set_min_proto_version(m_ctx, TLS1_2_VERSION);
    (void)major; (void)minor;
}

void TLSSocket::setMaxVersion(int major, int minor) {
    if (!m_ctx) return;
    SSL_CTX_set_max_proto_version(m_ctx, TLS1_3_VERSION);
    (void)major; (void)minor;
}

void TLSSocket::setSNIHostname(const std::string& hostname) {
    if (m_ssl) {
        SSL_set_tlsext_host_name(m_ssl, hostname.c_str());
    }
}

void TLSSocket::setALPNProtocols(const std::vector<std::string>& protocols) {
    if (!m_ctx) return;
    
    std::string protocolsStr;
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (i > 0) protocolsStr += ",";
        protocolsStr += protocols[i];
    }
    
    SSL_CTX_set_alpn_protos(m_ctx, 
        reinterpret_cast<const uint8_t*>(protocolsStr.data()),
        static_cast<int>(protocolsStr.size()));
}

void TLSSocket::connect(const std::string& host, uint16_t port) {
    // First do TCP connect
    Socket::connect(host, port);
    
    // Create SSL object
    m_ssl = SSL_new(m_ctx);
    SSL_set_fd(m_ssl, m_fd);
    
    // Set SNI hostname
    SSL_set_tlsext_host_name(m_ssl, host.c_str());
    
    // Perform handshake
    doHandshake();
}

void TLSSocket::accept() override {
    // Create SSL object
    m_ssl = SSL_new(m_ctx);
    SSL_set_fd(m_ssl, m_fd);
    
    // Perform handshake
    doHandshake();
}

bool TLSSocket::shutdown() {
    if (!m_ssl) return true;
    
    int ret = SSL_shutdown(m_ssl);
    if (ret == 0) {
        // Need to call again
        ret = SSL_shutdown(m_ssl);
    }
    
    return ret > 0;
}

size_t TLSSocket::send(std::span<const uint8_t> data) {
    return sslSend(data);
}

size_t TLSSocket::send(std::string_view data) {
    return send(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()
    ));
}

void TLSSocket::sendAll(std::span<const uint8_t> data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        size_t sent = send(data.subspan(totalSent));
        if (sent == 0) {
            throw TLSException("Connection closed during send");
        }
        totalSent += sent;
    }
}

size_t TLSSocket::recv(std::span<uint8_t> buffer) {
    return sslRecv(buffer);
}

std::vector<uint8_t> TLSSocket::recvAll(size_t maxSize) {
    std::vector<uint8_t> result;
    result.reserve(std::min(maxSize, size_t(65536)));
    
    while (result.size() < maxSize) {
        std::vector<uint8_t> chunk(16384);
        size_t received = recv(chunk);
        if (received == 0) {
            break;
        }
        result.insert(result.end(), chunk.begin(), chunk.begin() + received);
        
        if (received < chunk.size()) {
            break;
        }
    }
    
    return result;
}

std::vector<uint8_t> TLSSocket::recvExactly(size_t n) {
    std::vector<uint8_t> buffer(n);
    size_t totalReceived = 0;
    
    while (totalReceived < n) {
        size_t received = recv(std::span<uint8_t>(buffer.data() + totalReceived, n - totalReceived));
        if (received == 0) {
            throw TLSException("Connection closed before receiving all data");
        }
        totalReceived += received;
    }
    
    return buffer;
}

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
    
    char buffer[256];
    X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, buffer, sizeof(buffer));
    X509_free(cert);
    
    return buffer;
}

std::optional<std::string> TLSSocket::getPeerCertificateFingerprint() {
    if (!m_ssl) return std::nullopt;
    
    X509* cert = SSL_get_peer_certificate(m_ssl);
    if (!cert) return std::nullopt;
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    X509_digest(cert, EVP_sha256(), hash, &len);
    X509_free(cert);
    
    std::string result;
    for (unsigned int i = 0; i < len; ++i) {
        result += Utils::format("%02x:", hash[i]);
    }
    if (!result.empty()) {
        result.pop_back();
    }
    
    return result;
}

std::optional<std::string> TLSSocket::getALPNProtocol() {
    if (!m_ssl) return std::nullopt;
    
    const unsigned char* protocol = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(m_ssl, &protocol, &len);
    
    if (protocol && len > 0) {
        return std::string(reinterpret_cast<const char*>(protocol), len);
    }
    return std::nullopt;
}

SSL* TLSSocket::ssl() const {
    return m_ssl;
}

SSL_CTX* TLSSocket::context() const {
    return m_ctx;
}

void TLSSocket::initContext(TLSMode mode) {
    m_serverMode = (mode == TLSMode::SERVER);
    
    if (m_serverMode) {
        m_ctx = SSL_CTX_new(TLS_server_method());
    } else {
        m_ctx = SSL_CTX_new(TLS_client_method());
    }
    
    if (!m_ctx) {
        throw TLSException("Failed to create SSL context");
    }
    
    // Set default options
    SSL_CTX_set_options(m_ctx, SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    
    // Set minimum TLS 1.2
    SSL_CTX_set_min_proto_version(m_ctx, TLS1_2_VERSION);
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
    int ret = SSL_accept(m_ssl);
    
    if (ret <= 0) {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Non-blocking, would block
            // Should wait for socket to be ready
            return;
        }
        throw TLSException("TLS handshake failed: " + Crypto::lastError());
    }
    
    m_handshakeDone = true;
}

size_t TLSSocket::sslRecv(std::span<uint8_t> buffer) {
    int ret = SSL_read(m_ssl, buffer.data(), static_cast<int>(buffer.size()));
    
    if (ret <= 0) {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            return 0;  // Connection closed
        }
        throw TLSException("SSL read failed: " + Crypto::lastError());
    }
    
    return static_cast<size_t>(ret);
}

size_t TLSSocket::sslSend(std::span<const uint8_t> data) {
    int ret = SSL_write(m_ssl, data.data(), static_cast<int>(data.size()));
    
    if (ret <= 0) {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;
        }
        throw TLSException("SSL write failed: " + Crypto::lastError());
    }
    
    return static_cast<size_t>(ret);
}

// ============================================================================
// TLS Context
// ============================================================================

TLSContext::TLSContext(SSL_CTX* ctx, bool serverMode)
    : m_ctx(ctx), m_serverMode(serverMode) {}

TLSContext::Server TLSContext::createServer(const std::string& certFile, const std::string& keyFile) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        throw TLSException("Failed to create TLS context");
    }
    
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        throw TLSException("Failed to load certificate");
    }
    
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        throw TLSException("Failed to load private key");
    }
    
    return std::shared_ptr<TLSContext>(new TLSContext(ctx, true));
}

TLSContext::Client TLSContext::createClient() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw TLSException("Failed to create TLS context");
    }
    
    return std::shared_ptr<TLSContext>(new TLSContext(ctx, false));
}

SSL_CTX* TLSContext::context() const {
    return m_ctx;
}

std::unique_ptr<TLSSocket> TLSContext::createSocket() {
    return std::make_unique<TLSSocket>();
}

// ============================================================================
// TLS Utilities
// ============================================================================

namespace TLS {

std::pair<std::string, std::string> generateSelfSigned(
    const std::string& commonName,
    const std::string& organization
) {
    // Generate key pair
    EVP_PKEY* pkey = EVP_PKEY_new();
    RSA* rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
    EVP_PKEY_assign_RSA(pkey, rsa);
    
    // Create certificate
    X509* cert = X509_new();
    X509_set_version(cert, 2);  // v3
    
    // Set serial
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    
    // Set validity
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 60 * 60);  // 1 year
    
    // Set subject
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, 
        (unsigned char*)commonName.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8,
        (unsigned char*)organization.c_str(), -1, -1, 0);
    
    // Set issuer (self-signed)
    X509_set_issuer_name(cert, name);
    
    // Set public key
    X509_set_pubkey(cert, pkey);
    
    // Sign
    X509_sign(cert, pkey, EVP_sha256());
    
    // Write to PEM strings
    BIO* bio = BIO_new(BIO_s_mem());
    
    PEM_write_bio_X509(bio, cert);
    char* ptr = nullptr;
    long len = BIO_get_mem_data(bio, &ptr);
    std::string certStr(ptr, len);
    BIO_free(bio);
    
    bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    len = BIO_get_mem_data(bio, &ptr);
    std::string keyStr(ptr, len);
    BIO_free(bio);
    
    X509_free(cert);
    EVP_PKEY_free(pkey);
    
    return {certStr, keyStr};
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> loadCertAndKey(
    const std::string& certFile,
    const std::string& keyFile
) {
    std::vector<uint8_t> cert, key;
    
    auto certData = Utils::readFile(certFile);
    if (certData) cert = *certData;
    
    auto keyData = Utils::readFile(keyFile);
    if (keyData) key = *keyData;
    
    return {cert, key};
}

} // namespace TLS

} // namespace OmniNet
