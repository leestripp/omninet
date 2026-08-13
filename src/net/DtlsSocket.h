// OmniNet - DTLS datagram socket (encrypted UDP) for game networking
//
// Building-block API: wraps one UDP socket in an OpenSSL DTLS session.
// Server: bind() + accept() learns the client address from the first
// datagram. Client: connect(). Then send()/recv() encrypt/decrypt via
// SSL_write/SSL_read. Blocking with timeouts (not pump-driven like
// GameConnection); run on its own thread for real-time use.
//
// Note: stateless cookie exchange (DTLSv1_listen) is NOT wired yet — see
// Plan.md "Future" for the blocker analysis. A public server should front
// this with rate limiting. Certificates reuse the same PEM files as
// the TLS stack; generate with omninet-certs or Crypto::generateSelfSignedCert.
#ifndef OMNINET_DTLS_SOCKET_H
#define OMNINET_DTLS_SOCKET_H
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "net/UdpSocket.h"
#include "utils/Expected.h"

#include <openssl/ssl.h>

namespace OmniNet {

class DtlsSocket {
public:
    DtlsSocket() = default;
    ~DtlsSocket();

    DtlsSocket(const DtlsSocket&) = delete;
    DtlsSocket& operator=(const DtlsSocket&) = delete;

    // --- Server ---
    [[nodiscard]] Result<void, SocketError> bind(const std::string& address, uint16_t port);
    // Wait for a client and complete the DTLS handshake
    [[nodiscard]] Result<void, SocketError> accept(const std::string& certFile,
                                                   const std::string& keyFile,
                                                   int timeoutMs = 10000);

    // --- Client ---
    // verifyPeer=false: accept self-signed server certs (game default)
    [[nodiscard]] Result<void, SocketError> connect(const std::string& host, uint16_t port,
                                                    int timeoutMs = 10000,
                                                    bool verifyPeer = false);

    // --- I/O (both sides, after handshake) ---
    [[nodiscard]] Result<size_t, SocketError> send(std::span<const uint8_t> data);
    [[nodiscard]] Result<std::vector<uint8_t>, SocketError> recv(size_t maxSize = 65536,
                                                                 int timeoutMs = 1000);

    void close();
    bool isOpen() const { return m_fd >= 0; }
    bool isEstablished() const { return m_established; }
    uint16_t localPort() const;
    UdpEndpoint peer() const { return m_peer; }
    std::string cipherName() const;

private:
    Result<void, SocketError> openUdp();
    bool waitReadable(int timeoutMs) const;


    SOCKET m_fd = -1;
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_established = false;
    UdpEndpoint m_peer;
};

} // namespace OmniNet

#endif // OMNINET_DTLS_SOCKET_H
