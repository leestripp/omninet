// OmniNet - Async Task (C++20 coroutine return type)
// Lazy/symmetric coroutine task with result passing and awaitable support
#ifndef OMNINET_ASYNC_TASK_H
#define OMNINET_ASYNC_TASK_H

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <utility>

#include "Awaitable.h"

namespace OmniNet::Async {

// Forward declaration
template<typename T> class Task;

// ============================================================================
// Task<T>
// ============================================================================
//
// A lazy coroutine return type that:
// - Stores the result (or void)
// - Supports co_await on other tasks
// - Tracks completion state
// - Can be converted to std::future<T> for sync code
//
// Usage:
//   Async::Task<int> compute_async() {
//       co_return 42;
//   }
//
//   Task<void> chain() {
//       int v = co_await compute_async();
//       co_return;
//   }
//
template<typename T = void>
class Task {
public:
    // Promise type for the coroutine machinery
    struct promise_type {
        std::optional<T> m_value;
        std::exception_ptr m_exception;
        std::coroutine_handle<> m_continuation = nullptr;

        Task get_return_object() noexcept {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // Suspend initially - task is lazy and runs when resumed
        std::suspend_always initial_suspend() noexcept { return {}; }

        // Always suspend at the end, so we can resume the continuation
        std::suspend_always final_suspend() noexcept { return {}; }

        // Store return value
        template<typename U>
        void return_value(U&& value) {
            m_value = std::forward<U>(value);
        }

        // Catch unhandled exceptions
        void unhandled_exception() noexcept {
            m_exception = std::current_exception();
        }

        // Get the result, throwing if there's an error
        T& result() & {
            if (m_exception) {
                std::rethrow_exception(m_exception);
            }
            if (!m_value) {
                throw std::runtime_error("Task has no value");
            }
            return *m_value;
        }

        T&& result() && {
            if (m_exception) {
                std::rethrow_exception(m_exception);
            }
            if (!m_value) {
                throw std::runtime_error("Task has no value");
            }
            return std::move(*m_value);
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    // Default constructor (empty task)
    Task() noexcept : m_handle(nullptr) {}

    // Construct from coroutine handle
    explicit Task(handle_type h) noexcept : m_handle(h) {}

    // Destructor
    ~Task() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    // Non-copyable
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Movable
    Task(Task&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    // ===== Awaitable interface =====

    // Is the result already available?
    bool await_ready() const noexcept {
        if (!m_handle) return true;
        return m_handle.done();
    }

    // Called when coroutine is suspended waiting for this task.
    // We arrange for our task's final_suspend to resume us.
    template<typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> waiter) noexcept {
        if (!m_handle) return true;

        // Set up the waiter as the continuation
        m_handle.promise().m_continuation = waiter;

        // Resume our task if not done yet
        if (!m_handle.done()) {
            m_handle.resume();
        }
        return false;  // Don't actually suspend the waiter
    }

    // Get the result after resumption
    T await_resume() {
        if (!m_handle) {
            throw std::runtime_error("Cannot await empty Task");
        }
        return std::move(m_handle.promise()).result();
    }

    // ===== Direct access =====

    // Check if the task has completed
    bool is_ready() const noexcept {
        return !m_handle || m_handle.done();
    }

    // Get the result (blocking, throws on error)
    T& result() & {
        if (!m_handle) {
            throw std::runtime_error("Task has no handle");
        }
        return m_handle.promise().result();
    }

    T&& result() && {
        if (!m_handle) {
            throw std::runtime_error("Task has no handle");
        }
        return std::move(m_handle.promise()).result();
    }

    // Resume the coroutine manually
    void resume() {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
        }
    }

    // Get the underlying coroutine handle
    handle_type handle() const noexcept { return m_handle; }

    // Release ownership of the handle
    handle_type release() noexcept {
        handle_type h = m_handle;
        m_handle = nullptr;
        return h;
    }

private:
    handle_type m_handle;
};

// ============================================================================
// Task<void> specialization
// ============================================================================
template<>
class Task<void> {
public:
    struct promise_type {
        std::exception_ptr m_exception;
        std::coroutine_handle<> m_continuation = nullptr;

        Task get_return_object() noexcept {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}
        void unhandled_exception() noexcept {
            m_exception = std::current_exception();
        }

        void result() {
            if (m_exception) {
                std::rethrow_exception(m_exception);
            }
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Task() noexcept : m_handle(nullptr) {}
    explicit Task(handle_type h) noexcept : m_handle(h) {}

    ~Task() {
        if (m_handle) m_handle.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    // Awaitable
    bool await_ready() const noexcept {
        return !m_handle || m_handle.done();
    }

    template<typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> waiter) noexcept {
        if (!m_handle) return true;
        m_handle.promise().m_continuation = waiter;
        if (!m_handle.done()) {
            m_handle.resume();
        }
        return false;
    }

    void await_resume() {
        if (!m_handle) {
            throw std::runtime_error("Cannot await empty Task");
        }
        m_handle.promise().result();
    }

    bool is_ready() const noexcept {
        return !m_handle || m_handle.done();
    }

    void result() {
        if (!m_handle) {
            throw std::runtime_error("Task has no handle");
        }
        m_handle.promise().result();
    }

    void resume() {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
        }
    }

    handle_type handle() const noexcept { return m_handle; }
    handle_type release() noexcept {
        handle_type h = m_handle;
        m_handle = nullptr;
        return h;
    }

private:
    handle_type m_handle;
};

// ============================================================================
// Helper: blocking_get
// ============================================================================
//
// Synchronously wait for an async task to complete and return the result.
// Used to integrate async code with sync code (e.g., main(), tests).
//
template<typename T>
T blocking_get(Task<T> task) {
    task.resume();
    return std::move(task).result();
}

template<>
inline void blocking_get(Task<void> task) {
    task.resume();
    task.result();
}

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_TASK_H
