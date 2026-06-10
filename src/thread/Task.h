// OmniNet - Thread Pool Task Type
// C++23 std::move_only_function wrapper for type-erased callable tasks
#ifndef OMNINET_THREAD_TASK_H
#define OMNINET_THREAD_TASK_H

#include <functional>
#include <utility>

namespace OmniNet::Thread {

// Task is a type-erased, move-only callable that takes no arguments
// and returns void. C++23's std::move_only_function is used for
// zero-overhead type erasure without the small-object optimization
// limitations of std::function.
using Task = std::move_only_function<void()>;

// Helper to create a Task from any callable
template<typename F>
Task make_task(F&& f) {
    return Task{std::forward<F>(f)};
}

} // namespace OmniNet::Thread

#endif // OMNINET_THREAD_TASK_H
