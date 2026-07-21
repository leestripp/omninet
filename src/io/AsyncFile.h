// OmniNet - AsyncFile
// Non-blocking file I/O using ThreadPool (cross-platform)
#ifndef OMNINET_IO_ASYNC_FILE_H
#define OMNINET_IO_ASYNC_FILE_H

#include <unistd.h>    // ::close(), ::read(), ::write(), ::fsync(), ::lseek()

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "../async/Executor.h"
#include "../async/Task.h"

namespace OmniNet::IO {

// ============================================================================
// AsyncFile
// ============================================================================
//
// Asynchronous file I/O. Uses ThreadPool for cross-platform compatibility
// (no io_uring dependency, works on all POSIX systems).
//
// The blocking read()/write() syscalls are dispatched to the executor so
// the calling thread is never blocked. The coroutine resumes when the
// operation completes.
//
// Usage:
//   AsyncFile file(executor);
//   co_await file.open("/data/big.txt");
//   auto contents = co_await file.read_all();
//   co_await file.close();
//
class AsyncFile {
public:
    explicit AsyncFile(Async::Executor& exec);
    ~AsyncFile();

    // Non-copyable
    AsyncFile(const AsyncFile&) = delete;
    AsyncFile& operator=(const AsyncFile&) = delete;

    // Movable
    AsyncFile(AsyncFile&& other) noexcept
        : m_exec(other.m_exec), m_fd(other.m_fd), m_path(std::move(other.m_path)) {
        other.m_fd = -1;
    }
    AsyncFile& operator=(AsyncFile&& other) noexcept {
        if (this != &other) {
            if (m_fd >= 0) ::close(m_fd);
            m_fd = other.m_fd;
            m_path = std::move(other.m_path);
            m_lastError = std::move(other.m_lastError);
            other.m_fd = -1;
        }
        return *this;
    }

    // ----- File operations -----

    // Open a file (async via thread pool)
    // mode: "r" for read (default), "w" for write, "a" for append
    Async::Task<> open(std::string_view path, std::string_view mode = "r");

    // Read entire file contents
    Async::Task<std::vector<uint8_t>> read_all();

    // Read up to length bytes from offset
    Async::Task<std::vector<uint8_t>> read_range(size_t offset, size_t length);

    // Write data to file (overwrites at current position)
    Async::Task<size_t> write(std::span<const uint8_t> data);

    // Write data at specific offset
    Async::Task<size_t> write_at(size_t offset, std::span<const uint8_t> data);

    // Close the file
    Async::Task<> close();

    // ----- State -----

    bool isOpen() const;
    int fd() const { return m_fd; }
    const std::string& path() const { return m_path; }
    size_t size() const;

    // ----- Error handling -----

    // Get the last error message (empty if no error)
    const std::string& lastError() const;

private:
    Async::Executor& m_exec;
    int m_fd = -1;
    std::string m_path;
    std::string m_lastError;
};

} // namespace OmniNet::IO

#endif // OMNINET_IO_ASYNC_FILE_H
