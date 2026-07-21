// OmniNet - Streaming HTTP Response Writer
#ifndef OMNINET_HTTP_STREAMING_RESPONSE_H
#define OMNINET_HTTP_STREAMING_RESPONSE_H

#include "net/Socket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

namespace OmniNet::HTTP {

// ============================================================================
// Streaming Response
// ============================================================================
//
// Request-scoped writer for HTTP chunked-transfer responses. Created by
// HTTP::Response::startStreaming() and returned as a shared_ptr so the
// application can write chunks from worker threads after the HTTP handler
// has returned.
//
// The HTTP connection writes the response headers, then hands the socket
// to attach(). From that point on the application owns the stream via the
// shared_ptr and can write()/writeSseEvent()/close() from any thread.
//
// Thread-safety: write(), writeSseEvent(), close(), client_connected(),
// and on_disconnect() are all safe to call concurrently.

class StreamingResponse {
public:
    StreamingResponse();
    ~StreamingResponse();

    // Non-copyable, non-movable
    StreamingResponse(const StreamingResponse&) = delete;
    StreamingResponse& operator=(const StreamingResponse&) = delete;

    // Called exactly once by the HTTP server after the response headers
    // have been sent. Takes ownership of the socket.
    void attach(std::unique_ptr<Socket> socket);

    // Thread-safe chunked writes. The caller provides the raw payload;
    // chunked-transfer framing is added automatically.
    void write(std::string_view data);
    void write(std::span<const uint8_t> data);

    // Convenience helper for Server-Sent Events.
    // Writes one SSE event of the form:
    //   event: <eventName>\r\n
    //   data: <jsonData>\r\n
    //   \r\n
    // as a single chunked frame.
    void writeSseEvent(std::string_view eventName, std::string_view jsonData);

    // Signal end-of-stream and close the underlying socket.
    void close();

    // Returns true until a disconnect is detected or close() is called.
    bool client_connected() const;

    // Register a one-shot callback that is invoked when the client
    // disconnects or when a write fails. If the stream is already
    // disconnected when this is called, the callback fires immediately.
    void on_disconnect(std::function<void()> callback);

private:
    void monitorLoop();
    void setDisconnected();
    bool writeChunked(std::string_view data);
    void joinMonitorThread();

    // Protects m_socket, m_connected (assignment), m_onDisconnect.
    mutable std::mutex m_mutex;
    // Protects the monitor thread handle lifecycle (assign/join).
    mutable std::mutex m_threadMutex;
    std::unique_ptr<Socket> m_socket;

    std::atomic<bool> m_attached{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_closed{false};

    std::function<void()> m_onDisconnect;
    std::atomic<bool> m_disconnectCallbackFired{false};

    std::thread m_monitorThread;
};

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_STREAMING_RESPONSE_H
