// OmniNet - Async HTTP Handler
// Async route handler signature using C++20 coroutines
#ifndef OMNINET_ASYNC_ASYNC_HANDLER_H
#define OMNINET_ASYNC_ASYNC_HANDLER_H

#include <functional>
#include <future>
#include <utility>

#include "Executor.h"
#include "Task.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace OmniNet::Async {

// ============================================================================
// AsyncHandler
// ============================================================================
//
// A route handler that returns a Task<Response>. The task is scheduled on
// the provided Executor and runs asynchronously without blocking the calling
// thread (typically an event loop or thread pool).
//
// Example:
//   server.get("/async", [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
//       auto data = co_await exec.submit([]() {
//           return db.query("SELECT ...");
//       });
//       HTTP::Response resp;
//       resp.setBody(std::move(data));
//       co_return resp;
//   });
//
using AsyncHandler = std::function<Task<HTTP::Response>(
    const HTTP::Request&,
    Executor&
)>;

// ============================================================================
// FutureHandler
// ============================================================================
//
// A handler that returns std::future<Response>. More flexible than Task
// but doesn't have coroutine support.
//
using FutureHandler = std::function<std::future<HTTP::Response>(
    const HTTP::Request&,
    Executor&
)>;

} // namespace OmniNet::Async

#endif // OMNINET_ASYNC_ASYNC_HANDLER_H
