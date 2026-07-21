// OmniNet - Epoll-based I/O Context Implementation
#include "EpollContext.h"

#include <algorithm>
#include <iostream>

#ifdef __linux__

namespace OmniNet::IO {

// ============================================================================
// Construction / Destruction
// ============================================================================

EpollIoContext::EpollIoContext(Async::Executor& exec) : m_executor(exec) {
    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0) {
        throw std::runtime_error("Failed to create epoll file descriptor");
    }
}

EpollIoContext::~EpollIoContext() {
    stop();
    if (m_epollFd >= 0) {
        close(m_epollFd);
        m_epollFd = -1;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void EpollIoContext::run() {
    run(std::stop_token{});
}

void EpollIoContext::run(std::stop_token st) {
    m_running.store(true);
    m_stopping.store(false);

    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (m_running.load() && !m_stopping.load() && !st.stop_requested()) {
        // Process posted tasks
        {
            std::vector<Thread::Task> tasks;
            {
                std::lock_guard<std::mutex> lock(m_postedMutex);
                tasks.swap(m_postedTasks);
            }
            for (auto& task : tasks) {
                m_executor.try_submit(std::move(task));
            }
        }

        // Process due timers
        process_timers();

        // Compute timeout based on next timer
        int timeoutMs = compute_timeout_ms(std::chrono::milliseconds(100));

        // Wait for events
        int count = epoll_wait(m_epollFd, events, MAX_EVENTS, timeoutMs);
        if (count < 0) {
            if (errno == EINTR) continue;  // Interrupted by signal
            std::cerr << "[EpollIoContext] epoll_wait error: "
                      << errno << std::endl;
            break;
        }

        // Dispatch events
        dispatch_events(events, count);
    }

    m_running.store(false);
}

std::chrono::milliseconds EpollIoContext::run_once(std::chrono::milliseconds timeout) {
    // Process posted tasks
    {
        std::vector<Thread::Task> tasks;
        {
            std::lock_guard<std::mutex> lock(m_postedMutex);
            tasks.swap(m_postedTasks);
        }
        for (auto& task : tasks) {
            m_executor.try_submit(std::move(task));
        }
    }

    // Process due timers
    process_timers();

    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    auto start = std::chrono::steady_clock::now();
    int count = epoll_wait(m_epollFd, events, MAX_EVENTS,
                            static_cast<int>(timeout.count()));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    if (count < 0) {
        if (errno == EINTR) return elapsed;
        return elapsed;
    }

    dispatch_events(events, count);
    return elapsed;
}

void EpollIoContext::stop() {
    m_stopping.store(true);
    m_running.store(false);
}

// ============================================================================
// Posting
// ============================================================================

void EpollIoContext::post(Thread::Task task) {
    std::lock_guard<std::mutex> lock(m_postedMutex);
    m_postedTasks.push_back(std::move(task));
}

void EpollIoContext::resume(std::coroutine_handle<> handle) {
    Thread::Task task = [handle]() {
        handle.resume();
    };
    post(std::move(task));
}

// ============================================================================
// Socket watching
// ============================================================================

uint64_t EpollIoContext::watch(Socket& socket, EventType events, EventCallback callback) {
    int fd = socket.fd();
    if (fd < 0) return 0;

    uint32_t epollEvents = 0;
    if (static_cast<bool>(events & EventType::Read))    epollEvents |= EPOLLIN;
    if (static_cast<bool>(events & EventType::Write))   epollEvents |= EPOLLOUT;
    if (static_cast<bool>(events & EventType::Error))   epollEvents |= EPOLLERR;
    if (static_cast<bool>(events & EventType::Hangup))  epollEvents |= EPOLLHUP;

    uint64_t watchId = 0;
    {
        std::lock_guard<std::mutex> lock(m_watchesMutex);
        watchId = m_nextWatchId++;
        m_watches[watchId] = Watch{fd, events, std::move(callback)};
    }

    // Use watchId as the epoll data (we look up callback by fd->watchId mapping)
    struct epoll_event ev;
    ev.events = epollEvents;
    // Pack watchId in the data union (8 bytes for the pointer, but we need to be careful)
    // For simplicity, just use the fd as the data and look up by fd
    ev.data.fd = fd;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::lock_guard<std::mutex> lock(m_watchesMutex);
        m_watches.erase(watchId);
        return 0;
    }

    // Store watchId -> fd mapping in a separate map for cleanup
    // (omitted for brevity - in production, use a more sophisticated scheme)

    return watchId;
}

void EpollIoContext::unwatch(uint64_t watchId) {
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(m_watchesMutex);
        auto it = m_watches.find(watchId);
        if (it == m_watches.end()) return;
        fd = it->second.fd;
        m_watches.erase(it);
    }

    if (fd >= 0) {
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    }
}

// ============================================================================
// Timers
// ============================================================================

uint64_t EpollIoContext::timer_after(std::chrono::milliseconds delay,
                                      std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_timersMutex);
    uint64_t id = m_nextTimerId++;
    m_timers.push_back(Timer{
        id,
        std::chrono::steady_clock::now() + delay,
        std::move(callback),
        false
    });
    return id;
}

void EpollIoContext::timer_cancel(uint64_t timerId) {
    std::lock_guard<std::mutex> lock(m_timersMutex);
    for (auto& t : m_timers) {
        if (t.id == timerId) {
            t.cancelled = true;
            return;
        }
    }
}

void EpollIoContext::process_timers() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::function<void()>> toRun;

    {
        std::lock_guard<std::mutex> lock(m_timersMutex);
        // Find due timers
        auto it = m_timers.begin();
        while (it != m_timers.end()) {
            if (it->cancelled) {
                it = m_timers.erase(it);
            } else if (it->expiry <= now) {
                toRun.push_back(std::move(it->callback));
                it = m_timers.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Run callbacks outside the lock
    for (auto& cb : toRun) {
        cb();
    }
}

int EpollIoContext::compute_timeout_ms(std::chrono::milliseconds maxWait) {
    std::lock_guard<std::mutex> lock(m_timersMutex);
    if (m_timers.empty()) {
        return static_cast<int>(maxWait.count());
    }

    auto now = std::chrono::steady_clock::now();
    auto nextExpiry = std::min_element(
        m_timers.begin(), m_timers.end(),
        [](const Timer& a, const Timer& b) { return a.expiry < b.expiry; }
    )->expiry;

    if (nextExpiry <= now) return 0;

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(nextExpiry - now);
    auto capped = std::min(diff, maxWait);
    return static_cast<int>(capped.count());
}

// ============================================================================
// Event dispatch
// ============================================================================

void EpollIoContext::dispatch_events(struct epoll_event* events, int count) {
    // Collect callbacks to invoke outside the lock
    std::vector<std::pair<int, EventType>> triggered;

    for (int i = 0; i < count; ++i) {
        int fd = events[i].data.fd;
        EventType ev = EventType::None;
        if (static_cast<bool>(events[i].events & EPOLLIN))  ev |= EventType::Read;
        if (static_cast<bool>(events[i].events & EPOLLOUT)) ev |= EventType::Write;
        if (static_cast<bool>(events[i].events & EPOLLERR)) ev |= EventType::Error;
        if (static_cast<bool>(events[i].events & EPOLLHUP)) ev |= EventType::Hangup;
        triggered.push_back({fd, ev});
    }

    // Dispatch callbacks
    for (auto& [fd, ev] : triggered) {
        std::vector<EventCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_watchesMutex);
            for (auto& [id, watch] : m_watches) {
                if (watch.fd == fd) {
                    callbacks.push_back(watch.callback);
                }
            }
        }
        for (auto& cb : callbacks) {
            try {
                cb(ev);
            } catch (const std::exception& e) {
                std::cerr << "[EpollIoContext] Callback exception: "
                          << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[EpollIoContext] Callback unknown exception"
                          << std::endl;
            }
        }
    }
}

} // namespace OmniNet::IO

#endif // __linux__
