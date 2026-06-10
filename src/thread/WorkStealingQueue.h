// OmniNet - Work Stealing Queue
// Per-worker thread-safe deque with steal-from-front semantics
#ifndef OMNINET_THREAD_WORK_STEALING_QUEUE_H
#define OMNINET_THREAD_WORK_STEALING_QUEUE_H

#include <deque>
#include <mutex>
#include <optional>

#include "Task.h"

namespace OmniNet::Thread {

// Per-worker work queue with work-stealing support.
//
// Semantics:
// - Owner (the worker thread) calls push_back() and pop_back() for LIFO
//   access (good for cache locality).
// - Thieves (other workers) call steal() which pops from the front (FIFO)
//   to reduce contention with the owner.
//
// All operations are thread-safe via a single mutex. For higher contention
// scenarios, consider a lock-free MPMC queue.
class WorkStealingQueue {
public:
    WorkStealingQueue() = default;

    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    // Push to the back (owner-side).
    void push(Task task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(task));
    }

    // Pop from the back (owner-side, LIFO).
    // Returns std::nullopt if empty.
    std::optional<Task> pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        Task task = std::move(m_queue.back());
        m_queue.pop_back();
        return task;
    }

    // Steal from the front (thief-side, FIFO).
    // Returns std::nullopt if empty.
    std::optional<Task> steal() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        Task task = std::move(m_queue.front());
        m_queue.pop_front();
        return task;
    }

    // Check if empty (for stealing decisions).
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    // Approximate size (for statistics).
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<Task> m_queue;
};

} // namespace OmniNet::Thread

#endif // OMNINET_THREAD_WORK_STEALING_QUEUE_H
