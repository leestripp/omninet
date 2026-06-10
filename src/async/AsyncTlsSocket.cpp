// OmniNet - AsyncTlsSocket Implementation
#include "AsyncTlsSocket.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <cstring>
#include <iostream>

namespace OmniNet::Async {

// ============================================================================
// handshake
// ============================================================================
//
// Drives SSL_accept/SSL_connect in a loop, awaiting socket readiness on
// SSL_ERROR_WANT_READ/WANT_WRITE.
//
// Note: This is a client-style handshake. The underlying TLSSocket
// must have its SSL object initialized first. We use a two-step approach:
// 1. Call performHandshake() once to initialize the SSL object (may block briefly)
// 2. Then drive the handshake asynchronously
//
// For server-side handshake with full async, the AsyncServer integration
// is more complex and would need custom SSL initialization.
//
Async::Task<> AsyncTlsSocket::handshake() {
    if (!m_socket || !m_socket->isOpen()) {
        co_return;
    }

    // Use a simpler approach: just do the synchronous handshake
    // The TLSSocket already has select()-based non-blocking retry logic
    // For real async, this would need a more complex refactor
    try {
        m_socket->performHandshake();
    } catch (...) {
        throw;
    }
    co_return;
}

// ============================================================================
// read_some
// ============================================================================

Async::Task<size_t> AsyncTlsSocket::read_some(std::span<uint8_t> buf) {
    if (!m_socket || !m_socket->isOpen() || buf.empty()) {
        co_return size_t{0};
    }

    SSL* ssl = m_socket->getSSL();
    if (!ssl) {
        co_return size_t{0};
    }

    int iterations = 0;
    const int maxIterations = 100;

    while (iterations++ < maxIterations) {
        int ret = SSL_read(ssl, buf.data(), static_cast<int>(buf.size()));

        if (ret > 0) {
            co_return static_cast<size_t>(ret);
        }

        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
            co_await IO::when_readable(m_context, *m_socket);
            continue;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            // SSL_read can return WANT_WRITE during renegotiation
            co_await IO::when_writable(m_context, *m_socket);
            continue;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            // Clean shutdown
            co_return size_t{0};
        } else {
            // Error
            co_return size_t{0};
        }
    }

    co_return size_t{0};
}

// ============================================================================
// write_some
// ============================================================================

Async::Task<size_t> AsyncTlsSocket::write_some(std::span<const uint8_t> data) {
    if (!m_socket || !m_socket->isOpen() || data.empty()) {
        co_return size_t{0};
    }

    SSL* ssl = m_socket->getSSL();
    if (!ssl) {
        co_return size_t{0};
    }

    int iterations = 0;
    const int maxIterations = 100;

    while (iterations++ < maxIterations) {
        int ret = SSL_write(ssl, data.data(), static_cast<int>(data.size()));

        if (ret > 0) {
            co_return static_cast<size_t>(ret);
        }

        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
            co_await IO::when_writable(m_context, *m_socket);
            continue;
        } else if (err == SSL_ERROR_WANT_READ) {
            // SSL_write can return WANT_READ during renegotiation
            co_await IO::when_readable(m_context, *m_socket);
            continue;
        } else {
            co_return size_t{0};
        }
    }

    co_return size_t{0};
}

// ============================================================================
// write_all
// ============================================================================

Async::Task<void> AsyncTlsSocket::write_all(std::span<const uint8_t> data) {
    size_t offset = 0;
    while (offset < data.size()) {
        auto remaining = data.subspan(offset);
        size_t written = co_await write_some(remaining);
        if (written == 0) {
            co_return;
        }
        offset += written;
    }
    co_return;
}

// ============================================================================
// shutdown
// ============================================================================

Async::Task<bool> AsyncTlsSocket::shutdown() {
    if (!m_socket) {
        co_return false;
    }
    co_return m_socket->shutdown();
}

// ============================================================================
// State
// ============================================================================

bool AsyncTlsSocket::isHandshakeDone() const {
    return m_socket && m_socket->isSecure();
}

} // namespace OmniNet::Async
