// OmniNet - Async TLS Socket
// Coroutine-friendly wrapper around TLSSocket using IoContext
#ifndef OMNINET_ASYNC_ASYNC_TLS_SOCKET_H
#define OMNINET_ASYNC_ASYNC_TLS_SOCKET_H

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "../async/Task.h"
#include "../io/AsyncSocket.h"
#include "../io/IoContext.h"
#include "../net/TLSSocket.h"

namespace OmniNet::Async {

// ============================================================================
// AsyncTlsSocket
// ============================================================================
//
// Coroutine-friendly wrapper around TLSSocket. Uses IoContext for non-blocking
// I/O so the event loop is not blocked during TLS operations.
//
// Usage:
//   AsyncTlsSocket tls(ctx, std::move(tcpSocket));
//   tls.setCertFile("server.crt");
//   tls.setKeyFile("server.key");
//   co_await tls.handshake();  // Non-blocking TLS handshake
//
//   std::array<uint8_t, 4096> buf;
//   size_t n = co_await tls.read_some(buf);
//   co_await tls.write_all({buf.data(), n});
//
class AsyncTlsSocket {
public:
    // Create from existing TLSSocket (must be non-blocking or close to)
    AsyncTlsSocket(IO::IoContext& ctx, std::unique_ptr<TLSSocket> socket)
        : m_context(ctx), m_socket(std::move(socket)) {
        if (m_socket) {
            m_socket->setNonBlocking(true);
        }
    }

    // Move only
    AsyncTlsSocket(AsyncTlsSocket&& other) noexcept
        : m_context(other.m_context), m_socket(std::move(other.m_socket)) {}

    AsyncTlsSocket& operator=(AsyncTlsSocket&& other) noexcept {
        if (this != &other) {
            m_socket = std::move(other.m_socket);
        }
        return *this;
    }
    AsyncTlsSocket(const AsyncTlsSocket&) = delete;
    AsyncTlsSocket& operator=(const AsyncTlsSocket&) = delete;

    ~AsyncTlsSocket() = default;

    // ----- Configuration (delegates to underlying TLSSocket) -----

    void setCertFile(const std::string& path) { m_socket->setCertFile(path); }
    void setKeyFile(const std::string& path) { m_socket->setKeyFile(path); }
    void setCaFile(const std::string& path) { m_socket->setCaFile(path); }
    void setCaDir(const std::string& path) { m_socket->setCaDir(path); }
    void setVerifyMode(TLSVerify mode) { m_socket->setVerifyMode(mode); }
    void setMinVersion(int major, int minor) { m_socket->setMinVersion(major, minor); }
    void setMaxVersion(int major, int minor) { m_socket->setMaxVersion(major, minor); }
    void setSNIHostname(const std::string& hostname) { m_socket->setSNIHostname(hostname); }
    void setALPNProtocols(const std::vector<std::string>& protocols) {
        m_socket->setALPNProtocols(protocols);
    }

    // ----- Async I/O -----

    // Perform non-blocking TLS handshake
    // Loops calling SSL_accept/SSL_connect, awaiting socket readiness
    Async::Task<> handshake();

    // Read up to buf.size() bytes (0 = EOF or error)
    Async::Task<size_t> read_some(std::span<uint8_t> buf);

    // Write up to data.size() bytes
    Async::Task<size_t> write_some(std::span<const uint8_t> data);

    // Write all data (loops on partial writes)
    Async::Task<void> write_all(std::span<const uint8_t> data);

    // ----- State -----

    bool isHandshakeDone() const;
    bool isSecure() const { return m_socket && m_socket->isSecure(); }
    bool isOpen() const { return m_socket && m_socket->isOpen(); }

    // Shutdown TLS gracefully
    Async::Task<bool> shutdown();

    // Get underlying socket
    TLSSocket& native() { return *m_socket; }
    const TLSSocket& native() const { return *m_socket; }

    int fd() const { return m_socket ? m_socket->fd() : -1; }

    // Get the IoContext
    IO::IoContext& context() { return m_context; }

    // Cancel pending operations (not implemented for TLS)
    void cancel() {
        // TODO: track pending I/O and cancel
    }

private:
    IO::IoContext& m_context;
    std::unique_ptr<TLSSocket> m_socket;
};

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_ASYNC_TLS_SOCKET_H
