// OmniNet - Executor Interface
// Abstract executor for async tasks
#ifndef OMNINET_ASYNC_EXECUTOR_H
#define OMNINET_ASYNC_EXECUTOR_H

#include <chrono>
#include <coroutine>
#include <functional>
#include <future>
#include <type_traits>

#include "../thread/Task.h"
#include "../thread/ThreadPool.h"

namespace OmniNet::Async {

// ============================================================================
// Executor
// ============================================================================
//
// Abstract interface for executing work. Used by async handlers to:
// - Submit work to a thread pool
// - Schedule delayed work
// - Run coroutine continuations
//
class Executor {
public:
    virtual ~Executor() = default;

    // Submit a callable, get a future
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Submit a Task (move-only function)
    virtual bool try_submit(Thread::Task task) = 0;

    // Schedule a coroutine handle for resumption
    virtual void resume(std::coroutine_handle<> handle) = 0;

    // Check if executor is shutting down
    virtual bool running() const = 0;

    // Get the stop token (for cancellable work)
    virtual std::stop_token stop_token() const = 0;
};

// ============================================================================
// ThreadPoolExecutor
// ============================================================================
//
// Concrete Executor backed by a ThreadPool.
//
class ThreadPoolExecutor final : public Executor {
public:
    explicit ThreadPoolExecutor(Thread::ThreadPool& pool) : m_pool(pool) {}

    bool try_submit(Thread::Task task) override {
        return m_pool.try_submit(std::move(task));
    }

    void resume(std::coroutine_handle<> handle) override {
        m_pool.submit_coroutine(handle);
    }

    bool running() const override { return m_pool.running(); }

    std::stop_token stop_token() const override { return m_pool.stop_token(); }

private:
    Thread::ThreadPool& m_pool;
};

// Template implementation
template<typename F, typename... Args>
auto Executor::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    // Default implementation: dispatch via try_submit using packaged_task
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    Thread::Task task = [promise, f = std::forward<F>(f),
                          args = std::tuple<Args...>(std::forward<Args>(args)...)]() mutable {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                std::apply(f, std::move(args));
                promise->set_value();
            } else {
                promise->set_value(std::apply(f, std::move(args)));
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    if (!try_submit(std::move(task))) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("Executor queue full or not running")));
    }
    return future;
}

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_EXECUTOR_H
