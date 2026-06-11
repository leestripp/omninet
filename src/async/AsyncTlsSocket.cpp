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
// v0.6.0: Truly async TLS handshake. Loops calling SSL_accept/SSL_connect
// via TLSSocket::stepHandshake(), awaiting socket readiness via the
// IoContext event loop when the underlying socket isn't ready (instead
// of blocking in select() like the old TLSSocket::performHandshake()).
//
Async::Task<> AsyncTlsSocket::handshake() {
    if (!m_socket || !m_socket->isOpen()) {
        co_return;
    }

    // Initialize the SSL* (idempotent; does nothing if already created)
    m_socket->initHandshake();

    // Drive the handshake one step at a time, awaiting readiness on the
    // event loop thread when needed (the key behavior: this coroutine
    // yields control to the event loop instead of blocking in select()).
    constexpr int maxIterations = 1000;  // safety bound to prevent infinite loops
    for (int i = 0; i < maxIterations; ++i) {
        auto state = m_socket->stepHandshake();
        switch (state) {
            case TLSSocket::HandshakeState::Done:
                co_return;
            case TLSSocket::HandshakeState::WantRead:
                co_await IO::when_readable(m_context, *m_socket);
                break;
            case TLSSocket::HandshakeState::WantWrite:
                co_await IO::when_writable(m_context, *m_socket);
                break;
            case TLSSocket::HandshakeState::Error:
                throw std::runtime_error("TLS handshake failed (see OpenSSL errors)");
        }
    }

    throw std::runtime_error("TLS handshake timed out");
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
