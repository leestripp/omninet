// OmniNet - Awaitable Interface
// Base interface for awaiter types
#ifndef OMNINET_ASYNC_AWAITABLE_H
#define OMNINET_ASYNC_AWAITABLE_H

#include <coroutine>
#include <exception>
#include <optional>

namespace OmniNet::Async {

// ============================================================================
// AwaiterBase
// ============================================================================
//
// Common base for awaiter types. Provides the standard three await methods.
//
struct AwaiterBase {
    // Check if the result is already available (skip suspension)
    bool await_ready() const noexcept { return false; }

    // Called when coroutine is suspended; can schedule resumption
    void await_suspend(std::coroutine_handle<> h) noexcept {
        // Default: resume immediately on a different thread is non-trivial
        // Subclasses override this
        h.resume();
    }
};

// ============================================================================
// ReadyAwaiter
// ============================================================================
//
// An awaiter that is always ready. Used to allow co_await on Task<T> when
// the task has already completed.
//
struct ReadyAwaiter {
    bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_AWAITABLE_H
