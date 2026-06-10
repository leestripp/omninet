// OmniNet - I/O Context (Abstract Event Loop)
// Cross-platform event loop interface for non-blocking I/O
#ifndef OMNINET_IO_IO_CONTEXT_H
#define OMNINET_IO_IO_CONTEXT_H

#include <chrono>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <stop_token>
#include <vector>

#include "../async/Task.h"
#include "../async/Executor.h"
#include "../net/Socket.h"

namespace OmniNet::IO {

// Forward declarations
class IoContext;
class AsyncSocket;

// ============================================================================
// Event Types
// ============================================================================

enum class EventType : uint32_t {
    None    = 0,
    Read    = 1 << 0,
    Write   = 1 << 1,
    Error   = 1 << 2,
    Hangup  = 1 << 3,
};

inline EventType operator|(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EventType& operator|=(EventType& a, EventType b) {
    a = a | b;
    return a;
}

inline EventType operator&(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// ============================================================================
// Callback types
// ============================================================================

// Called when an event occurs. Receives the IoContext for posting continuations.
using EventCallback = std::function<void(EventType)>;

// ============================================================================
// IoContext (Abstract Base)
// ============================================================================
//
// Event loop interface. Concrete implementations:
//   - EpollIoContext (Linux)
//   - KqueueIoContext (BSD/macOS)
//   - IocpContext (Windows)
//
class IoContext {
public:
    virtual ~IoContext() = default;

    // ----- Lifecycle -----

    // Run the event loop (blocking, returns when stopped or stop_token fires)
    virtual void run() = 0;
    virtual void run(std::stop_token st) = 0;

    // Run one iteration of the event loop (non-blocking).
    // Returns the time spent waiting (0 if events were immediate).
    virtual std::chrono::milliseconds run_once(std::chrono::milliseconds timeout) = 0;

    // Stop the event loop
    virtual void stop() = 0;

    // ----- Posting -----

    // Schedule a callback to run in the event loop
    virtual void post(Thread::Task task) = 0;

    // Schedule a coroutine handle for resumption in the event loop
    virtual void resume(std::coroutine_handle<> handle) = 0;

    // ----- Socket watching -----

    // Watch a socket for events. Callback is called when any of the events occur.
    // Returns a watch ID that can be used to stop watching.
    virtual uint64_t watch(Socket& socket, EventType events, EventCallback callback) = 0;

    // Stop watching a socket (by watch ID)
    virtual void unwatch(uint64_t watchId) = 0;

    // ----- Timers -----

    // Schedule a callback after a delay. Returns a timer ID.
    virtual uint64_t timer_after(std::chrono::milliseconds delay, std::function<void()> callback) = 0;

    // Cancel a pending timer
    virtual void timer_cancel(uint64_t timerId) = 0;

    // ----- Introspection -----

    // Get the executor associated with this context
    virtual Async::Executor& executor() = 0;

    // Check if the context is currently running
    virtual bool running() const = 0;
};

// ============================================================================
// Awaitable event
// ============================================================================
//
// Coroutine awaiter that suspends until a socket event occurs.
//
class EventAwaitable {
public:
    EventAwaitable(IoContext& ctx, Socket& sock, EventType events)
        : m_context(ctx), m_socket(sock), m_events(events) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        m_watchId = m_context.watch(m_socket, m_events,
            [handle](EventType) mutable {
                handle.resume();
            });
    }

    EventType await_resume() {
        m_context.unwatch(m_watchId);
        return m_events;  // Simplified - we know which events we asked for
    }

private:
    IoContext& m_context;
    Socket& m_socket;
    EventType m_events;
    uint64_t m_watchId = 0;
};

// Helper to make an event awaitable
inline EventAwaitable when_readable(IoContext& ctx, Socket& sock) {
    return EventAwaitable(ctx, sock, EventType::Read | EventType::Error | EventType::Hangup);
}

inline EventAwaitable when_writable(IoContext& ctx, Socket& sock) {
    return EventAwaitable(ctx, sock, EventType::Write | EventType::Error | EventType::Hangup);
}

} // namespace OmniNet::IO

#endif // OMNINET_IO_IO_CONTEXT_H
