// OmniNet - Async Socket Implementation
#include "AsyncSocket.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace OmniNet::IO {

// ============================================================================
// read_some
// ============================================================================
//
// Reads up to buf.size() bytes. Suspends the coroutine until the socket
// is readable, then performs a non-blocking read.
//
Async::Task<size_t> AsyncSocket::read_some(std::span<uint8_t> buf) {
    if (buf.empty() || !m_socket || !m_socket->isOpen()) {
        co_return 0;
    }

    // Loop until we get data or error
    while (true) {
        // Try non-blocking read first
        ssize_t n = ::recv(m_socket->fd(), buf.data(), buf.size(), MSG_DONTWAIT);
        if (n > 0) {
            co_return static_cast<size_t>(n);
        }
        if (n == 0) {
            co_return size_t{0};  // EOF
        }

        // n < 0
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error
            co_return size_t{0};
        }

        // Would block - wait for readability
        co_await when_readable(m_context, *m_socket);
    }
}

// ============================================================================
// write_some
// ============================================================================

Async::Task<size_t> AsyncSocket::write_some(std::span<const uint8_t> data) {
    if (data.empty() || !m_socket || !m_socket->isOpen()) {
        co_return 0;
    }

    while (true) {
        ssize_t n = ::send(m_socket->fd(), data.data(), data.size(), MSG_DONTWAIT);
        if (n >= 0) {
            co_return static_cast<size_t>(n);
        }

        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            co_return size_t{0};
        }

        co_await when_writable(m_context, *m_socket);
    }
}

// ============================================================================
// write_all
// ============================================================================

Async::Task<void> AsyncSocket::write_all(std::span<const uint8_t> data) {
    size_t offset = 0;
    while (offset < data.size()) {
        auto remaining = data.subspan(offset);
        size_t written = co_await write_some(remaining);
        if (written == 0) {
            // Error or EOF
            co_return;
        }
        offset += written;
    }
    co_return;
}

} // namespace OmniNet::IO
