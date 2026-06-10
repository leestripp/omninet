// OmniNet - Epoll-based I/O Context (Linux)
// Linux epoll event loop implementation
#ifndef OMNINET_IO_EPOLL_CONTEXT_H
#define OMNINET_IO_EPOLL_CONTEXT_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <unordered_map>
#include <vector>

#include "../async/Task.h"
#include "../async/Executor.h"
#include "../net/Socket.h"
#include "IoContext.h"

#ifdef __linux__

#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

namespace OmniNet::IO {

// ============================================================================
// EpollIoContext
// ============================================================================
//
// Linux epoll-based event loop. Uses level-triggered mode (EPOLLLT) by default
// for simplicity. For high-performance scenarios, edge-triggered (EPOLLET) can
// be used but requires careful handling of partial reads/writes.
//
class EpollIoContext final : public IoContext {
public:
    explicit EpollIoContext(Async::Executor& exec);
    ~EpollIoContext() override;

    // Non-copyable
    EpollIoContext(const EpollIoContext&) = delete;
    EpollIoContext& operator=(const EpollIoContext&) = delete;

    // ----- Lifecycle -----

    void run() override;
    void run(std::stop_token st) override;
    std::chrono::milliseconds run_once(std::chrono::milliseconds timeout) override;
    void stop() override;

    // ----- Posting -----

    void post(Thread::Task task) override;
    void resume(std::coroutine_handle<> handle) override;

    // ----- Socket watching -----

    uint64_t watch(Socket& socket, EventType events, EventCallback callback) override;
    void unwatch(uint64_t watchId) override;

    // ----- Timers -----

    uint64_t timer_after(std::chrono::milliseconds delay, std::function<void()> callback) override;
    void timer_cancel(uint64_t timerId) override;

    // ----- Introspection -----

    Async::Executor& executor() override { return m_executor; }
    bool running() const override { return m_running.load(); }

    // Get the epoll file descriptor (for testing)
    int epoll_fd() const { return m_epollFd; }

private:
    // Watch entry
    struct Watch {
        int fd;
        EventType events;
        EventCallback callback;
    };

    // Timer entry
    struct Timer {
        uint64_t id;
        std::chrono::steady_clock::time_point expiry;
        std::function<void()> callback;
        bool cancelled = false;
    };

    // Dispatch events from epoll to callbacks
    void dispatch_events(struct epoll_event* events, int count);

    // Process due timers
    void process_timers();

    // Get next timer expiry (for epoll_wait timeout)
    int compute_timeout_ms(std::chrono::milliseconds maxWait);

    Async::Executor& m_executor;
    int m_epollFd = -1;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};

    // Watches
    std::mutex m_watchesMutex;
    std::unordered_map<uint64_t, Watch> m_watches;
    uint64_t m_nextWatchId = 1;

    // Posted tasks (FIFO)
    std::mutex m_postedMutex;
    std::vector<Thread::Task> m_postedTasks;

    // Timers
    std::mutex m_timersMutex;
    std::vector<Timer> m_timers;
    uint64_t m_nextTimerId = 1;

    // Timer thread (optional, for timer handling)
    // For simplicity, timers are processed during run_once
};

} // namespace OmniNet::IO

#endif // __linux__

#endif // OMNINET_IO_EPOLL_CONTEXT_H
