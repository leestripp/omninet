// OmniNet - Thread Pool Implementation
#include "ThreadPool.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <random>
#include <thread>

#include "utils/Exception.h"

namespace OmniNet::Thread {

// Chrono literals for use in this file
using namespace std::chrono_literals;

// ============================================================================
// Construction / Destruction
// ============================================================================

ThreadPool::ThreadPool() : ThreadPool(Config{}) {}

ThreadPool::ThreadPool(const Config& config) : m_config(config) {
    // Determine worker count
    if (m_config.workerCount == 0) {
        m_config.workerCount = std::thread::hardware_concurrency();
        if (m_config.workerCount == 0) {
            m_config.workerCount = 4;  // Fallback
        }
    }

    // Create worker queues
    m_queues.reserve(m_config.workerCount);
    for (size_t i = 0; i < m_config.workerCount; ++i) {
        m_queues.push_back(std::make_unique<WorkStealingQueue>());
    }

    // Start workers
    m_running.store(true);
    m_workers.reserve(m_config.workerCount);
    for (size_t i = 0; i < m_config.workerCount; ++i) {
        m_workers.emplace_back([this, i](std::stop_token st) {
            worker_loop(st, i);
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

void ThreadPool::shutdown() {
    bool wasRunning = m_running.exchange(false);
    if (!wasRunning) {
        return;
    }
    // Request stop on all jthreads (each has its own stop_source)
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
    m_cv.notify_all();
    // Wait for all workers to finish
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    m_queues.clear();
}

void ThreadPool::request_stop() {
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
    m_cv.notify_all();
}

// ============================================================================
// Task submission
// ============================================================================

bool ThreadPool::try_submit(Task task) {
    if (!m_running.load()) {
        m_totalRejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Check backpressure (non-blocking)
    if (m_queuedTasks.load(std::memory_order_relaxed) >= m_config.maxQueueSize) {
        m_totalRejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    enqueue_task(std::move(task));
    m_queuedTasks.fetch_add(1, std::memory_order_relaxed);
    m_cv.notify_one();
    return true;
}

void ThreadPool::submit_coroutine(std::coroutine_handle<> handle) {
    Task task = [handle]() {
        handle.resume();
    };
    submit(std::move(task));
}

// ============================================================================
// Worker loop
// ============================================================================

void ThreadPool::worker_loop(std::stop_token st, size_t workerId) {
    while (true) {
        // Quick exit check
        if (st.stop_requested() && m_queuedTasks.load() == 0) {
            break;
        }

        // Try to get a task from local queue or steal
        if (auto task = acquire_task(workerId)) {
            execute_task(std::move(*task));
            continue;
        }

        // No task available. Wait for work or stop request.
        std::unique_lock<std::mutex> lock(m_globalMutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(100), [this, &st] {
            return st.stop_requested() ||
                   m_queuedTasks.load(std::memory_order_relaxed) > 0;
        });
    }
}

std::optional<Task> ThreadPool::acquire_task(size_t workerId) {
    // 1. Try local queue (LIFO, pop from back)
    if (auto task = m_queues[workerId]->pop()) {
        m_queuedTasks.fetch_sub(1, std::memory_order_relaxed);
        return task;
    }

    // 2. Try to steal from peers (FIFO from front)
    if (m_config.stealWork && m_queues.size() > 1) {
        // Random starting point to spread load
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, m_queues.size() - 1);
        size_t startIdx = dist(rng);

        for (size_t i = 0; i < m_queues.size(); ++i) {
            size_t target = (startIdx + i) % m_queues.size();
            if (target == workerId) continue;

            if (auto task = m_queues[target]->steal()) {
                m_queuedTasks.fetch_sub(1, std::memory_order_relaxed);
                m_totalStolen.fetch_add(1, std::memory_order_relaxed);
                return task;
            }
        }
    }

    return std::nullopt;
}

void ThreadPool::enqueue_task(Task task) {
    if (m_queues.empty()) {
        // No workers; run inline (shouldn't happen in practice)
        execute_task(std::move(task));
        return;
    }

    // Round-robin distribution
    size_t target = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_queues.size();
    m_queues[target]->push(std::move(task));
}

void ThreadPool::execute_task(Task&& task) {
    m_activeTasks.fetch_add(1, std::memory_order_relaxed);
    try {
        task();
    } catch (const std::exception& e) {
        // Log and continue - don't let one bad task kill the pool
        std::cerr << "[ThreadPool] Uncaught exception in task: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[ThreadPool] Uncaught unknown exception in task" << std::endl;
    }
    m_activeTasks.fetch_sub(1, std::memory_order_relaxed);
    m_totalExecuted.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Statistics
// ============================================================================

ThreadPool::Stats ThreadPool::stats() const {
    Stats s;
    s.workerCount = m_workers.size();
    s.activeTasks = m_activeTasks.load(std::memory_order_relaxed);
    s.queuedTasks = m_queuedTasks.load(std::memory_order_relaxed);
    s.totalExecuted = m_totalExecuted.load(std::memory_order_relaxed);
    s.totalStolen = m_totalStolen.load(std::memory_order_relaxed);
    s.totalRejected = m_totalRejected.load(std::memory_order_relaxed);
    return s;
}

} // namespace OmniNet::Thread
