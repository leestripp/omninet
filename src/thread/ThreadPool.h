// OmniNet - Thread Pool
// High-performance thread pool with work-stealing and backpressure
#ifndef OMNINET_THREAD_THREAD_POOL_H
#define OMNINET_THREAD_THREAD_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Task.h"
#include "WorkStealingQueue.h"

namespace OmniNet::Thread {

// Chrono literals
using namespace std::chrono_literals;

// ============================================================================
// ThreadPool
// ============================================================================
//
// A work-stealing thread pool with the following features:
// - Configurable worker count (defaults to hardware_concurrency())
// - Per-worker lock-protected queues (good for low contention)
// - Work-stealing: idle workers steal from busy workers
// - Global overflow queue for backpressure handling
// - C++20 std::jthread with std::stop_token for cooperative cancellation
// - Future-based task submission with return values
// - Delayed task scheduling
// - Statistics tracking
//
// Threading model:
// - One std::jthread per worker
// - Each worker has its own WorkStealingQueue
// - Main thread submits tasks via submit()
// - New tasks go to a worker chosen by round-robin (poor man's distribution)
// - When local queue is empty, workers try to steal from peers
// - When all local queues are empty, workers wait on a condition variable
//
class ThreadPool {
public:
    // --- Configuration ---

    struct Config {
        // Number of worker threads. 0 = std::thread::hardware_concurrency()
        size_t workerCount = 0;

        // Maximum size of the global queue before backpressure kicks in.
        // submit() will block (or reject) when this is exceeded.
        size_t maxQueueSize = 1024;

        // Enable work stealing between worker queues
        bool stealWork = true;

        // Initial spin count before sleeping on condition variable.
        // Reduces wakeup latency for short bursts.
        size_t spinCount = 1000;
    };

    // --- Statistics ---

    struct Stats {
        size_t workerCount;
        size_t activeTasks;
        size_t queuedTasks;
        uint64_t totalExecuted;
        uint64_t totalStolen;
        uint64_t totalRejected;
    };

    // --- Construction ---

    explicit ThreadPool(const Config& config);
    ThreadPool();  // Default config

    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // --- Task submission ---

    // Submit a callable and get a future for the result.
    // Blocks if the global queue is full (backpressure).
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Submit a Task (move-only function) without blocking on backpressure.
    // Returns false if the queue is full.
    bool try_submit(Task task);

    // Schedule a callable to run after a delay.
    // Returns a future for the result.
    template<typename Rep, typename Period, typename F>
    auto submit_after(std::chrono::duration<Rep, Period> delay, F&& f)
        -> std::future<void>;

    // --- Lifecycle ---

    // Graceful shutdown - waits for all in-flight tasks to complete.
    // Safe to call multiple times.
    void shutdown();

    // Request cancellation (workers finish current task, then stop).
    // Use shutdown() if you want to wait for completion.
    void request_stop();

    // --- Introspection ---

    // Get current statistics
    Stats stats() const;

    // Get the number of worker threads
    size_t worker_count() const noexcept { return m_workers.size(); }

    // Check if the pool is currently running
    bool running() const noexcept { return m_running.load(); }

    // Get the stop token (default-constructed; use isRunning() to check state)
    std::stop_token stop_token() const noexcept {
        if (!m_workers.empty()) {
            return m_workers[0].get_stop_token();
        }
        return {};
    }

    // --- Coroutine support (Phase 3) ---

    // Submit a coroutine handle for execution
    void submit_coroutine(std::coroutine_handle<> handle);

private:
    // Worker thread main loop
    void worker_loop(std::stop_token st, size_t workerId);

    // Try to get a task from local queue, then steal from peers
    std::optional<Task> acquire_task(size_t workerId);

    // Add a task to a worker's queue (round-robin)
    void enqueue_task(Task task);

    // Run a single task (with active count tracking and exception handling)
    void execute_task(Task&& task);

    // Statistics
    mutable std::mutex m_statsMutex;
    std::atomic<size_t> m_activeTasks{0};
    std::atomic<uint64_t> m_totalExecuted{0};
    std::atomic<uint64_t> m_totalStolen{0};
    std::atomic<uint64_t> m_totalRejected{0};
    std::atomic<uint64_t> m_queuedTasks{0};

    // Worker queues
    std::vector<std::unique_ptr<WorkStealingQueue>> m_queues;
    std::vector<std::jthread> m_workers;

    // Round-robin counter for task distribution
    std::atomic<size_t> m_nextWorker{0};

    // Synchronization
    std::mutex m_globalMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running{false};

    Config m_config;
};

// ============================================================================
// Template implementations
// ============================================================================

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using ReturnType = std::invoke_result_t<F, Args...>;

    // Create a packaged task
    auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = taskPtr->get_future();

    // Wrap the packaged task in a Task
    Task task = [taskPtr]() {
        (*taskPtr)();
    };

    // Submit (blocks if backpressure)
    {
        std::unique_lock<std::mutex> lock(m_globalMutex);
        m_cv.wait(lock, [this] {
            return !m_running || m_queuedTasks.load() < m_config.maxQueueSize;
        });
        if (!m_running) {
            // Pool is shutting down, but still execute the task synchronously
            // to honor the future contract
            (*taskPtr)();
            return result;
        }
    }

    enqueue_task(std::move(task));
    m_queuedTasks.fetch_add(1, std::memory_order_relaxed);
    m_cv.notify_one();

    return result;
}

template<typename Rep, typename Period, typename F>
auto ThreadPool::submit_after(std::chrono::duration<Rep, Period> delay, F&& f)
    -> std::future<void>
{
    auto promise = std::make_shared<std::promise<void>>();
    std::future<void> result = promise->get_future();

    // Schedule a delayed task on a worker
    auto delayTask = [this, delay, f = std::forward<F>(f), promise]() {
        std::this_thread::sleep_for(delay);
        if (!m_running.load()) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("Pool stopped before delayed task could run")));
            return;
        }
        try {
            f();
            promise->set_value();
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    Task task = std::move(delayTask);
    submit(std::move(task));
    return result;
}

} // namespace OmniNet::Thread

#endif // OMNINET_THREAD_THREAD_POOL_H
