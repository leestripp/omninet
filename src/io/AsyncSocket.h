// OmniNet - Async Socket (Coroutine-friendly Socket)
// Wraps a Socket with async read/write/accept operations
#ifndef OMNINET_IO_ASYNC_SOCKET_H
#define OMNINET_IO_ASYNC_SOCKET_H

#include <coroutine>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "../async/Task.h"
#include "../net/Socket.h"
#include "IoContext.h"

namespace OmniNet::IO {

// ============================================================================
// AsyncSocket
// ============================================================================
//
// Coroutine-friendly wrapper around Socket. Read/write/accept operations
// suspend the coroutine until the underlying I/O is ready.
//
// Usage:
//   Async::Task<void> handle_connection(AsyncSocket sock) {
//       std::array<uint8_t, 4096> buf;
//       while (true) {
//           size_t n = co_await sock.read_some(buf);
//           if (n == 0) break;
//           co_await sock.write_all({buf.data(), n});
//       }
//   }
//
class AsyncSocket {
public:
    AsyncSocket(IoContext& ctx, std::unique_ptr<Socket> socket)
        : m_context(ctx), m_socket(std::move(socket)) {}

    // Non-copyable
    AsyncSocket(const AsyncSocket&) = delete;
    AsyncSocket& operator=(const AsyncSocket&) = delete;

    // Non-movable: holds an IoContext& reference, which can't be
    // rebound. Lifetime is tied to the IoContext the socket was
    // constructed against. Stored in std::unique_ptr by callers, so
    // move semantics aren't needed. (v0.8.7 was defaulting these,
    // which the compiler silently deleted -> warning.)
    AsyncSocket(AsyncSocket&&) noexcept = delete;
    AsyncSocket& operator=(AsyncSocket&&) noexcept = delete;

    ~AsyncSocket() = default;

    // ----- Async I/O -----

    // Read up to buf.size() bytes. Returns bytes actually read (0 = EOF).
    Async::Task<size_t> read_some(std::span<uint8_t> buf);

    // Write up to data.size() bytes. Returns bytes actually written.
    Async::Task<size_t> write_some(std::span<const uint8_t> data);

    // Write all data (loops on partial writes)
    Async::Task<void> write_all(std::span<const uint8_t> data);

    // ----- Native access -----

    Socket& native() { return *m_socket; }
    const Socket& native() const { return *m_socket; }
    int fd() const { return m_socket->fd(); }

    // Close the socket
    void close() {
        if (m_socket) {
            m_socket->close();
        }
    }

    // Get the IoContext
    IoContext& context() { return m_context; }

private:
    IoContext& m_context;
    std::unique_ptr<Socket> m_socket;
};

} // namespace OmniNet::IO

#endif // OMNINET_IO_ASYNC_SOCKET_H
