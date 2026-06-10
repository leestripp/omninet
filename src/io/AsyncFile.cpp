// OmniNet - AsyncFile Implementation
#include "AsyncFile.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <span>
#include <string>

namespace OmniNet::IO {

// ============================================================================
// Construction / Destruction
// ============================================================================

AsyncFile::AsyncFile(Async::Executor& exec) : m_exec(exec) {}

AsyncFile::~AsyncFile() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

// ============================================================================
// File operations
// ============================================================================

Async::Task<> AsyncFile::open(std::string_view path, std::string_view mode) {
    std::string pathStr(path);
    std::string modeStr(mode);

    auto fut = m_exec.submit([this, pathStr, modeStr]() {
        int flags = O_RDONLY;
        if (modeStr == "w") {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (modeStr == "a") {
            flags = O_WRONLY | O_CREAT | O_APPEND;
        } else if (modeStr == "r+") {
            flags = O_RDWR;
        }

        int fd = ::open(pathStr.c_str(), flags, 0644);
        if (fd < 0) {
            m_lastError = "open failed: " +
                          std::string(std::strerror(errno));
            return -1;
        }

        m_lastError.clear();
        return fd;
    });

    m_fd = fut.get();
    if (m_fd >= 0) {
        m_path = pathStr;
    }
    co_return;
}

Async::Task<std::vector<uint8_t>> AsyncFile::read_all() {
    if (m_fd < 0) {
        co_return std::vector<uint8_t>{};
    }

    // First, get file size via fstat
    auto sizeFut = m_exec.submit([this]() {
        struct stat st;
        if (::fstat(m_fd, &st) < 0) {
            return -1L;
        }
        return static_cast<long>(st.st_size);
    });

    long sz = sizeFut.get();
    if (sz < 0) {
        co_return std::vector<uint8_t>{};
    }

    if (sz == 0) {
        co_return std::vector<uint8_t>{};
    }

    // Read the whole file
    std::vector<uint8_t> buffer(static_cast<size_t>(sz));
    auto fut = m_exec.submit([this, &buffer]() -> ssize_t {
        size_t total = 0;
        while (total < buffer.size()) {
            ssize_t n = ::read(m_fd, buffer.data() + total,
                                buffer.size() - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1L;
            }
            if (n == 0) break;  // EOF
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    });

    ssize_t read = fut.get();
    if (read < 0) {
        co_return std::vector<uint8_t>{};
    }

    buffer.resize(static_cast<size_t>(read));
    co_return buffer;
}

Async::Task<std::vector<uint8_t>>
AsyncFile::read_range(size_t offset, size_t length) {
    if (m_fd < 0 || length == 0) {
        co_return std::vector<uint8_t>{};
    }

    std::vector<uint8_t> buffer(length);
    auto fut = m_exec.submit([this, offset, length, &buffer]() -> ssize_t {
        // Seek to offset
        if (::lseek(m_fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
            return -1L;
        }

        size_t total = 0;
        while (total < length) {
            ssize_t n = ::read(m_fd, buffer.data() + total, length - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1L;
            }
            if (n == 0) break;
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    });

    ssize_t read = fut.get();
    if (read < 0) {
        co_return std::vector<uint8_t>{};
    }

    buffer.resize(static_cast<size_t>(read));
    co_return buffer;
}

Async::Task<size_t> AsyncFile::write(std::span<const uint8_t> data) {
    if (m_fd < 0 || data.empty()) {
        co_return size_t{0};
    }

    auto fut = m_exec.submit([this, data]() -> ssize_t {
        size_t total = 0;
        while (total < data.size()) {
            ssize_t n = ::write(m_fd, data.data() + total,
                                 data.size() - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1L;
            }
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    });

    ssize_t written = fut.get();
    if (written < 0) {
        co_return size_t{0};
    }
    co_return static_cast<size_t>(written);
}

Async::Task<size_t> AsyncFile::write_at(size_t offset, std::span<const uint8_t> data) {
    if (m_fd < 0 || data.empty()) {
        co_return size_t{0};
    }

    auto fut = m_exec.submit([this, offset, data]() -> ssize_t {
        if (::lseek(m_fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
            return -1L;
        }

        size_t total = 0;
        while (total < data.size()) {
            ssize_t n = ::write(m_fd, data.data() + total,
                                 data.size() - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1L;
            }
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    });

    ssize_t written = fut.get();
    if (written < 0) {
        co_return size_t{0};
    }
    co_return static_cast<size_t>(written);
}

Async::Task<> AsyncFile::close() {
    if (m_fd < 0) {
        co_return;
    }

    int fd = m_fd;
    m_fd = -1;
    m_path.clear();

    auto fut = m_exec.submit([fd]() {
        return ::close(fd) == 0;
    });
    fut.wait();
    co_return;
}

// ============================================================================
// State
// ============================================================================

bool AsyncFile::isOpen() const {
    return m_fd >= 0;
}

size_t AsyncFile::size() const {
    if (m_fd < 0) return 0;
    struct stat st;
    if (::fstat(m_fd, &st) < 0) return 0;
    return static_cast<size_t>(st.st_size);
}

std::string AsyncFile::lastError() const {
    return m_lastError;
}

} // namespace OmniNet::IO
