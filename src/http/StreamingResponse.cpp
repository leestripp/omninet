// OmniNet - Streaming HTTP Response Writer Implementation
#include "http/StreamingResponse.h"

#include <chrono>
#include <mutex>
#include <sstream>

namespace OmniNet::HTTP {

// ============================================================================
// Streaming Response
// ============================================================================

StreamingResponse::StreamingResponse() = default;

StreamingResponse::~StreamingResponse() {
    close();
}

void StreamingResponse::attach(std::unique_ptr<Socket> socket) {
    bool expected = false;
    if (!m_attached.compare_exchange_strong(expected, true)) {
        // Already attached — drop the new socket.
        return;
    }

    // If close() was called before the HTTP server handed off the socket,
    // do not start the monitor thread. Just shut down the socket and exit.
    if (m_closed.load()) {
        if (socket && socket->isOpen()) {
            try {
                socket->shutdown(SHUT_RDWR);
            } catch (...) {
            }
            try {
                socket->close();
            } catch (...) {
            }
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket = std::move(socket);
        m_connected = m_socket && m_socket->isOpen();
    }

    std::thread monitor([this] { monitorLoop(); });
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_monitorThread = std::move(monitor);
    }
}

void StreamingResponse::write(std::string_view data) {
    if (m_closed.load() || !m_connected.load()) {
        return;
    }
    if (!writeChunked(data)) {
        setDisconnected();
    }
}

void StreamingResponse::write(std::span<const uint8_t> data) {
    write(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

void StreamingResponse::writeSseEvent(std::string_view eventName,
                                      std::string_view jsonData) {
    std::ostringstream oss;
    oss << "event: " << eventName << "\r\n";

    // SSE allows multi-line data; prefix every line with "data: ".
    size_t start = 0;
    while (start <= jsonData.size()) {
        size_t newline = jsonData.find('\n', start);
        std::string_view line = (newline == std::string_view::npos)
            ? jsonData.substr(start)
            : jsonData.substr(start, newline - start);
        oss << "data: " << line << "\r\n";
        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }
    oss << "\r\n";

    write(oss.str());
}

void StreamingResponse::close() {
    if (m_closed.exchange(true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket && m_socket->isOpen()) {
            try {
                static constexpr std::string_view terminator = "0\r\n\r\n";
                m_socket->sendAll(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(terminator.data()),
                    terminator.size()));
            } catch (...) {
                // Ignore errors during close; we're shutting down anyway.
            }
            // Shutdown before joining the monitor thread so a monitor
            // thread blocked in waitForRead() is woken up reliably.
            try {
                m_socket->shutdown(SHUT_RDWR);
            } catch (...) {
                // Ignore.
            }
        }
    }

    setDisconnected();

    // Join the monitor thread BEFORE closing the socket. This guarantees
    // the monitor thread never calls waitForRead() (which reads m_fd)
    // concurrently with Socket::close() (which writes m_fd).
    joinMonitorThread();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket && m_socket->isOpen()) {
            try {
                m_socket->close();
            } catch (...) {
                // Ignore.
            }
        }
    }
}

void StreamingResponse::joinMonitorThread() {
    std::lock_guard<std::mutex> lock(m_threadMutex);
    if (m_monitorThread.joinable()) {
        try {
            m_monitorThread.join();
        } catch (...) {
            // Ignore.
        }
    }
}

bool StreamingResponse::client_connected() const {
    return m_connected.load() && !m_closed.load();
}

void StreamingResponse::on_disconnect(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_onDisconnect = std::move(callback);
    }

    // If already disconnected, fire immediately (once).
    if (!m_connected.load()) {
        bool fired = false;
        if (m_disconnectCallbackFired.compare_exchange_strong(fired, true)) {
            std::function<void()> cb;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                cb = m_onDisconnect;
            }
            if (cb) {
                cb();
            }
        }
    }
}

void StreamingResponse::monitorLoop() {
    while (!m_closed.load()) {
        Socket* sock = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_socket || !m_socket->isOpen()) {
                break;
            }
            sock = m_socket.get();
        }

        bool readable = false;
        try {
            // Short timeout so close()/shutdown() are not the only way
            // to wake the monitor thread.
            readable = sock->waitForRead(std::chrono::seconds(1));
        } catch (...) {
            break;
        }

        if (m_closed.load()) {
            break;
        }
        if (!readable) {
            continue;
        }

        try {
            auto data = sock->recvAll(1);
            if (data.empty()) {
                // Peer closed the connection (real EOF).
                break;
            }
            // Streaming is server->client; ignore any client data.
        } catch (...) {
            break;
        }
    }

    setDisconnected();
}

void StreamingResponse::setDisconnected() {
    const bool wasConnected = m_connected.exchange(false);
    if (!wasConnected) {
        return;
    }

    bool fired = false;
    if (!m_disconnectCallbackFired.compare_exchange_strong(fired, true)) {
        return;
    }

    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_onDisconnect;
    }

    if (cb) {
        cb();
    }
}

bool StreamingResponse::writeChunked(std::string_view data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_closed.load() || !m_socket || !m_socket->isOpen()) {
        return false;
    }

    try {
        std::ostringstream headerOss;
        headerOss << std::hex << data.size() << "\r\n";
        const auto header = headerOss.str();

        m_socket->sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(header.data()), header.size()));
        m_socket->sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()), data.size()));
        m_socket->sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>("\r\n"), 2));
    } catch (...) {
        return false;
    }

    return true;
}

} // namespace OmniNet::HTTP
