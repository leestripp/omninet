// OmniNet - DTLS datagram socket (encrypted UDP) for game networking
//
// Building-block API: wraps one UDP socket in an OpenSSL DTLS session.
// Server: bind() + accept() learns the client address from the first
// datagram. Client: connect(). Then send()/recv() encrypt/decrypt via
// SSL_write/SSL_read. Blocking with timeouts (not pump-driven like
// GameConnection); run on its own thread for real-time use.
//
// DoS hardening: pass requireCookie=true to accept() for the stateless DTLS
// cookie exchange (HelloVerifyRequest): no handshake state is created until
// the client proves ownership of its source address. The fd is nonblocking
// and handshake loops honor DTLS retransmission timers.
// Certificates reuse the same PEM files as
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

#include <chrono>

#include <openssl/ssl.h>

namespace OmniNet {

using SteadyDeadline = std::chrono::steady_clock::time_point;

class DtlsSocket {
public:
    DtlsSocket() = default;
    ~DtlsSocket();

    DtlsSocket(const DtlsSocket&) = delete;
    DtlsSocket& operator=(const DtlsSocket&) = delete;

    // --- Server ---
    [[nodiscard]] Result<void, SocketError> bind(const std::string& address, uint16_t port);
    // Wait for a client and complete the DTLS handshake.
    // requireCookie: stateless address verification (DoS hardening).
    [[nodiscard]] Result<void, SocketError> accept(const std::string& certFile,
                                                   const std::string& keyFile,
                                                   int timeoutMs = 10000,
                                                   bool requireCookie = false);

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
    void setBlocking(bool blocking);
    // Drive SSL_accept/SSL_connect honoring the DTLS retransmission timer
    Result<void, SocketError> driveHandshake(bool server, SteadyDeadline deadline);

    // Cookie exchange (DoS hardening)
    void ensureCookieSecret();
    unsigned int makeCookie(unsigned char* out, unsigned int maxLen) const;
    bool checkCookie(const unsigned char* cookie, unsigned int len) const;
    static int cookieGenerateCb(SSL* ssl, unsigned char* cookie, unsigned int* len);
    static int cookieVerifyCb(SSL* ssl, const unsigned char* cookie, unsigned int len);

    SOCKET m_fd = -1;
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_established = false;
    UdpEndpoint m_peer;
    sockaddr_in m_peerAddr{};            // for the cookie HMAC
    std::vector<uint8_t> m_cookieSecret;
};

} // namespace OmniNet

#endif // OMNINET_DTLS_SOCKET_H
