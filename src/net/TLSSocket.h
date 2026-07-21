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
#include "crypto/Crypto.h"

#include <openssl/ssl.h>

namespace OmniNet {

// v0.7.2 (codemax Bug 1): forward declaration so TLSSocket can
// hold a std::shared_ptr<TLSContext>. The full definition is
// below (after the TLSSocket class).
class TLSContext;

// ============================================================================
// TLS Socket Exception (defined in Exception.h)
// ============================================================================

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

    // v0.7.2 (codemax Bug 1): constructor that uses a shared
    // TLSContext. The socket keeps a shared_ptr to the context
    // so the context outlives the socket. Used by
    // TLSContext::createSocket().
    explicit TLSSocket(std::shared_ptr<TLSContext> context);

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
    
    // Perform TLS handshake on existing connected socket (server)
    void performHandshake();
    
    // Shutdown TLS connection gracefully
    bool shutdown();
    
    // --- I/O (overridden) ---
    
    size_t send(std::span<const uint8_t> data) override;
    size_t send(std::string_view data) override;
    
    void sendAll(std::span<const uint8_t> data) override;
    
    size_t recv(std::span<uint8_t> buffer) override;
    
    std::vector<uint8_t> recvAll(size_t maxSize = 65536) override;
    
    std::vector<uint8_t> recvExactly(size_t n) override;

    // v0.7.2 (codemax Bug 3): override readLine() to use the
    // overridden recv() (which goes through SSL_read) instead of
    // the base class's raw ::recv(). Without this, calling
    // readLine() on a TLSSocket returned raw ciphertext (the TLS
    // record header), causing "malformed status line" errors
    // on any HTTPS request.
    std::optional<std::string> readLine(size_t maxLength = 65536) override;

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

    // ----- Internal accessors (for AsyncTlsSocket) -----

    // Get the underlying SSL handle (for advanced use)
    SSL* getSSL() const { return m_ssl; }

    // Check if this is server-mode
    bool isServerMode() const { return m_serverMode; }

    // Mark handshake as done (used after async handshake completes)
    void markHandshakeDone() { m_handshakeDone = true; }

    // v0.6.0: Result of a single handshake step (for AsyncTlsSocket)
    enum class HandshakeState {
        Done,        // Handshake complete
        WantRead,    // Need to wait for socket readability
        WantWrite,   // Need to wait for socket writeability
        Error        // Handshake failed (see OpenSSL error queue)
    };

    // Initialize the SSL* if not already (no handshake step). Required
    // before calling stepHandshake().
    void initHandshake();

    // Perform one handshake step. Returns the resulting state. Caller
    // should loop: on WantRead/WantWrite, await socket readiness, then
    // call again. On Done, handshake is complete. On Error, see
    // ERR_get_error() for details.
    HandshakeState stepHandshake();

    // Get the fd of the underlying socket
    int getRawFd() const { return m_fd; }
    std::optional<std::string> getALPNProtocol();
    
    // --- SSL Object Access (for advanced use) ---
    
    SSL* ssl() const;
    SSL_CTX* context() const;

private:
    void initContext(TLSMode mode);
    void cleanupContext();
    // v0.7.2 (codemax Bug 1): adopt a shared TLSContext. Frees
    // our own m_ctx (if any) and uses the shared context's SSL_CTX.
    void setContext(std::shared_ptr<TLSContext> context);
    
    // Perform TLS handshake
    void doHandshake();
    
    // v0.7.2 (codemax Bug 1): shared TLSContext. Set when the
    // socket is constructed from a TLSContext (via createSocket()
    // or the TLSSocket(shared_ptr<TLSContext>) constructor).
    // When non-null, m_ctx belongs to the shared context, not to
    // this socket.
    std::shared_ptr<TLSContext> m_context;

    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_serverMode = false;
    bool m_handshakeDone = false;
    
    // Keep underlying socket alive so fd isn't closed
    std::unique_ptr<Socket> m_underlyingSocket;
    
    // Configuration paths
    std::string m_certFile;
    std::string m_keyFile;
    std::string m_caFile;
    std::string m_caDir;
    std::string m_sniHostname;
};

// ============================================================================
// TLS Context Factory
// ============================================================================

// v0.7.2 (codemax Bug 1 + 5): TLSContext inherits
// std::enable_shared_from_this so that createSocket() can obtain
// a shared_ptr to *this via shared_from_this() and pass it to
// the new TLSSocket constructor.
class TLSContext : public std::enable_shared_from_this<TLSContext> {
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

    // v0.7.2: is this a server context? Used by TLSSocket to set
    // its initial mode when adopting a shared context.
    bool serverMode() const;

    // Create socket from this context
    std::unique_ptr<TLSSocket> createSocket();

    ~TLSContext();

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
