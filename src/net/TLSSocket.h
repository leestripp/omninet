// OmniNet - TLS/SSL Socket wrapper using OpenSSL
#ifndef OMNINET_TLS_SOCKET_H
#define OMNINET_TLS_SOCKET_H

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/Socket.h"

// Forward OpenSSL declarations
struct ssl_st;
struct ssl_ctx_st;
struct bio_st;

namespace OmniNet {

// ============================================================================
// TLS Socket Exception
// ============================================================================

class TLSException : public Exception {
public:
    TLSException(const std::string& message, int errorCode = 0);
};

// ============================================================================
// TLS Socket Class
// ============================================================================

class TLSSocket : public Socket {
public:
    // --- Constructors ---
    
    // Create client-mode TLS socket
    TLSSocket();
    
    // Create server-mode TLS socket (from accepted socket)
    explicit TLSSocket(std::unique_ptr<Socket> acceptedSocket);
    
    // Wrap existing socket with TLS
    explicit TLSSocket(SOCKET fd, bool serverMode);
    
    // Destructor
    ~TLSSocket() override;
    
    // --- Configuration ---
    
    // Set certificate file (PEM)
    void setCertFile(const std::string& path);
    
    // Set private key file (PEM)
    void setKeyFile(const std::string& path);
    
    // Set CA certificate file (for client cert verification)
    void setCaFile(const std::string& path);
    
    // Set CA directory (for client cert verification)
    void setCaDir(const std::string& path);
    
    // Set verification mode
    void setVerifyMode(TLSVerify mode);
    
    // Set minimum TLS version
    void setMinVersion(int major, int minor);
    
    // Set maximum TLS version
    void setMaxVersion(int major, int minor);
    
    // Set SNI hostname (client-side)
    void setSNIHostname(const std::string& hostname);
    
    // Set alpn protocols (e.g., "h2", "http/1.1")
    void setALPNProtocols(const std::vector<std::string>& protocols);
    
    // --- Connection ---
    
    // Connect and perform TLS handshake (client)
    void connect(const std::string& host, uint16_t port) override;
    
    // Accept connection and perform TLS handshake (server)
    void accept() override;
    
    // Shutdown TLS connection gracefully
    bool shutdown();
    
    // --- I/O (overridden) ---
    
    size_t send(std::span<const uint8_t> data) override;
    size_t send(std::string_view data) override;
    
    void sendAll(std::span<const uint8_t> data) override;
    
    size_t recv(std::span<uint8_t> buffer) override;
    
    std::vector<uint8_t> recvAll(size_t maxSize = 65536) override;
    
    std::vector<uint8_t> recvExactly(size_t n) override;
    
    // --- State ---
    
    bool isSecure() const;
    
    // Get negotiated cipher suite
    std::string getCipher();
    
    // Get TLS version
    std::string getVersion();
    
    // Check if peer has certificate
    bool hasPeerCertificate();
    
    // Get peer certificate CN
    std::optional<std::string> getPeerCertificateCN();
    
    // Get peer certificate fingerprint
    std::optional<std::string> getPeerCertificateFingerprint();
    
    // Get ALPN negotiated protocol
    std::optional<std::string> getALPNProtocol();
    
    // --- SSL Object Access (for advanced use) ---
    
    SSL* ssl() const;
    SSL_CTX* context() const;

private:
    void initContext(TLSMode mode);
    void cleanupContext();
    
    // Perform TLS handshake
    void doHandshake();
    
    // Internal receive with SSL
    size_t sslRecv(std::span<uint8_t> buffer);
    
    // Internal send with SSL
    size_t sslSend(std::span<const uint8_t> data);
    
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_serverMode = false;
    bool m_handshakeDone = false;
    
    // Configuration paths
    std::string m_certFile;
    std::string m_keyFile;
    std::string m_caFile;
    std::string m_caDir;
};

// ============================================================================
// TLS Context Factory
// ============================================================================

class TLSContext {
public:
    // Create server context with certificate and key
    static std::shared_ptr<TLSContext> createServer(
        const std::string& certFile,
        const std::string& keyFile
    );
    
    // Create client context
    static std::shared_ptr<TLSContext> createClient();
    
    // Get SSL_CTX for creating sockets
    SSL_CTX* context() const;
    
    // Create socket from this context
    std::unique_ptr<TLSSocket> createSocket();
    
private:
    TLSContext(SSL_CTX* ctx, bool serverMode);
    
    SSL_CTX* m_ctx;
    bool m_serverMode;
};

// ============================================================================
// TLS Utilities
// ============================================================================

namespace TLS {

// Generate self-signed certificate and key
std::pair<std::string, std::string> generateSelfSigned(
    const std::string& commonName,
    const std::string& organization = "OmniNet"
);

// Generate DH parameters for key exchange
std::string generateDHParams(size_t bits = 2048);

// Load certificate and key from files
std::pair<std::vector<uint8_t>, std::vector<uint8_t>> loadCertAndKey(
    const std::string& certFile,
    const std::string& keyFile
);

// Verify certificate chain
bool verifyCertificate(
    const std::string& certFile,
    const std::string& caFile
);

} // namespace TLS

} // namespace OmniNet

#endif // OMNINET_TLS_SOCKET_H
