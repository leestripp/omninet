// OmniNet - Pump-driven DTLS peer over memory BIOs (no socket ownership)
//
// DtlsSession is the game-loop-friendly DTLS building block: it owns no
// socket. The caller shuttles encrypted datagrams between pollOutgoing()
// and its own UDP transport, and feeds received datagrams to inputDatagram().
// Handshake and encryption are driven explicitly (drive()), so it composes
// with GameServer/GameClient's pump() model. Datagram boundaries are
// preserved via OpenSSL 3's BIO_s_dgram_mem.
//
// DoS hardening: initServer(requireCookie=true) runs the stateless cookie
// exchange — no handshake state is committed until the client proves its
// address. Call setPeerAddress() before drive() so the cookie HMAC can be
// computed (IPv4 peers for now).
#ifndef OMNINET_DTLS_SESSION_H
#define OMNINET_DTLS_SESSION_H
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <netinet/in.h>

#include "net/UdpSocket.h"
#include "utils/Expected.h"

#include <openssl/ssl.h>

namespace OmniNet {

class DtlsSession {
public:
    DtlsSession() = default;
    ~DtlsSession();

    DtlsSession(const DtlsSession&) = delete;
    DtlsSession& operator=(const DtlsSession&) = delete;

    // --- Setup (one of initServer / initClient) ---
    [[nodiscard]] Result<void, SocketError> initServer(const std::string& certFile,
                                                       const std::string& keyFile,
                                                       bool requireCookie = false);
    [[nodiscard]] Result<void, SocketError> initClient(bool verifyPeer = false);

    // Server only: the transport-level peer address, for the cookie HMAC
    void setPeerAddress(const sockaddr_in& addr);

    // --- Transport-facing ---
    // Feed an encrypted datagram received from the peer
    void inputDatagram(std::span<const uint8_t> datagram);
    // Drain encrypted datagrams to send to the peer
    void pollOutgoing(std::vector<std::vector<uint8_t>>& out);

    // --- Handshake ---
    // Drive the handshake state machine (after input, and after timers)
    void drive();
    bool isEstablished() const { return m_established; }
    // Milliseconds until the DTLS retransmission timer fires; -1 when none
    int nextTimeoutMs() const;
    // Fire the DTLS timer (retransmits the current flight if due)
    void handleTimeout();

    // --- Application-facing (after isEstablished) ---
    [[nodiscard]] Result<void, SocketError> encrypt(std::span<const uint8_t> plaintext);
    void pollPlaintext(std::vector<std::vector<uint8_t>>& out);

    std::string cipherName() const;

private:
    Result<void, SocketError> makeSsl();
    void ensureCookieSecret();
    unsigned int makeCookie(unsigned char* out, unsigned int maxLen) const;
    bool checkCookie(const unsigned char* cookie, unsigned int len) const;
    static int cookieGenerateCb(SSL* ssl, unsigned char* cookie, unsigned int* len);
    static int cookieVerifyCb(SSL* ssl, const unsigned char* cookie, unsigned int len);

    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    BIO* m_rbio = nullptr;  // peer -> us (owned by m_ssl)
    BIO* m_wbio = nullptr;  // us -> peer (owned by m_ssl)
    bool m_isServer = false;
    bool m_requireCookie = false;
    bool m_cookieVerified = false;
    bool m_established = false;
    sockaddr_in m_peerAddr{};
    std::vector<uint8_t> m_cookieSecret;
};

} // namespace OmniNet

#endif // OMNINET_DTLS_SESSION_H
