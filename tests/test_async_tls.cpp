// OmniNet - AsyncTlsSocket Unit Tests
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "async/AsyncTlsSocket.h"
#include "async/Task.h"
#include "io/IoContext.h"
#include "io/EpollContext.h"
#include "async/Executor.h"
#include "thread/ThreadPool.h"
#include "net/TLSSocket.h"
#include "net/Socket.h"

using namespace OmniNet;
using namespace OmniNet::Async;
using namespace OmniNet::IO;
using namespace std::chrono_literals;

// ============================================================================
// Test Framework
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            run_test(#name, test_##name); \
        } \
    } g_registrar_##name; \
    static void test_##name()

void run_test(const char* name, void (*func)()) {
    std::cout << "  " << name << "... ";
    try {
        func();
        std::cout << "PASS" << std::endl;
        ++g_passed;
    } catch (const std::exception& e) {
        std::cout << "FAIL (" << e.what() << ")" << std::endl;
        ++g_failed;
    } catch (...) {
        std::cout << "FAIL (unknown exception)" << std::endl;
        ++g_failed;
    }
}

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("ASSERT_TRUE failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("ASSERT_EQ failed"); \
    } \
} while(0)

struct TestEnv {
    Thread::ThreadPool::Config poolCfg;
    std::unique_ptr<Thread::ThreadPool> pool;
    std::unique_ptr<ThreadPoolExecutor> exec;
    std::unique_ptr<EpollIoContext> ctx;

    TestEnv() {
        poolCfg.workerCount = 2;
        pool = std::make_unique<Thread::ThreadPool>(poolCfg);
        exec = std::make_unique<ThreadPoolExecutor>(*pool);
        ctx = std::make_unique<EpollIoContext>(*exec);
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST(construct_with_null_socket) {
    TestEnv env;
    // Constructing with null should not throw
    std::unique_ptr<TLSSocket> tls;
    AsyncTlsSocket asyncTls(*env.ctx, std::move(tls));
    ASSERT_EQ(asyncTls.fd(), -1);
    ASSERT_TRUE(!asyncTls.isOpen());
}

TEST(construct_with_socket) {
    TestEnv env;

    // Create a real socket (no TLS handshake yet)
    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    ASSERT_TRUE(sock->isOpen());

    AsyncTlsSocket asyncTls(*env.ctx, std::move(sock));
    ASSERT_TRUE(asyncTls.isOpen());
    ASSERT_EQ(asyncTls.fd(), sv[0]);

    asyncTls.native().close();
    ::close(sv[1]);
}

TEST(move_construct) {
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    AsyncTlsSocket a(*env.ctx, std::move(sock));
    ASSERT_TRUE(a.isOpen());

    AsyncTlsSocket b = std::move(a);
    ASSERT_TRUE(b.isOpen());
    ASSERT_EQ(b.fd(), sv[0]);

    ::close(sv[1]);
}

TEST(move_assign) {
    TestEnv env;

    int sv1[2], sv2[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv1), 0);
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv2), 0);

    auto sock1 = std::make_unique<TLSSocket>(sv1[0], false);
    auto sock2 = std::make_unique<TLSSocket>(sv2[0], false);

    AsyncTlsSocket a(*env.ctx, std::move(sock1));
    AsyncTlsSocket b(*env.ctx, std::move(sock2));

    a = std::move(b);
    ASSERT_EQ(a.fd(), sv2[0]);

    ::close(sv1[1]);
    ::close(sv2[1]);
}

TEST(native_access) {
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    AsyncTlsSocket asyncTls(*env.ctx, std::move(sock));

    TLSSocket& native = asyncTls.native();
    ASSERT_TRUE(native.isOpen());

    ::close(sv[1]);
}

TEST(context_access) {
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    AsyncTlsSocket asyncTls(*env.ctx, std::move(sock));

    ASSERT_TRUE(&asyncTls.context() == env.ctx.get());
    ASSERT_EQ(asyncTls.fd(), sv[0]);

    ::close(sv[1]);
}

TEST(is_secure_before_handshake) {
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    AsyncTlsSocket asyncTls(*env.ctx, std::move(sock));

    // Before handshake, isSecure should be false
    ASSERT_TRUE(!asyncTls.isSecure());
    ASSERT_TRUE(!asyncTls.isHandshakeDone());

    ::close(sv[1]);
}

TEST(non_blocking_set_on_construct) {
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], false);
    int originalFd = sock->fd();
    AsyncTlsSocket asyncTls(*env.ctx, std::move(sock));

    // After construction, the underlying socket should be non-blocking
    int flags = ::fcntl(originalFd, F_GETFL, 0);
    ASSERT_TRUE((flags & O_NONBLOCK) != 0);

    ::close(sv[1]);
}

TEST(read_some_returns_zero_on_null_socket) {
    TestEnv env;
    std::unique_ptr<TLSSocket> tls;
    AsyncTlsSocket asyncTls(*env.ctx, std::move(tls));

    auto task = [&]() -> Task<size_t> {
        std::array<uint8_t, 100> buf;
        co_return co_await asyncTls.read_some(buf);
    };

    Task<size_t> t = task();
    t.resume();
    ASSERT_EQ(t.result(), size_t{0});
}

TEST(write_some_returns_zero_on_null_socket) {
    TestEnv env;
    std::unique_ptr<TLSSocket> tls;
    AsyncTlsSocket asyncTls(*env.ctx, std::move(tls));

    auto task = [&]() -> Task<size_t> {
        const char* data = "test";
        co_return co_await asyncTls.write_some(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data), 4));
    };

    Task<size_t> t = task();
    t.resume();
    ASSERT_EQ(t.result(), size_t{0});
}

TEST(handshake_returns_immediately_on_null_socket) {
    TestEnv env;
    std::unique_ptr<TLSSocket> tls;
    AsyncTlsSocket asyncTls(*env.ctx, std::move(tls));

    auto task = [&]() -> Task<> {
        co_await asyncTls.handshake();
    };

    Task<> t = task();
    t.resume();
    // Should complete without throwing
}

TEST(tls_socket_handshake_state_enum) {
    // v0.6.0: Verify the new HandshakeState enum on TLSSocket
    // and that initHandshake/stepHandshake work as expected
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], true);  // server mode
    // Make the socket non-blocking so SSL_accept returns WANT_READ
    // immediately instead of blocking waiting for a ClientHello
    sock->setNonBlocking(true);
    // initHandshake should not throw and should create the SSL*
    sock->initHandshake();
    ASSERT_TRUE(sock->getSSL() != nullptr);
    // Initial step on a socketpair where the peer hasn't done anything
    // should return WantRead (server waiting for ClientHello)
    auto state = sock->stepHandshake();
    ASSERT_EQ(state, TLSSocket::HandshakeState::WantRead);

    ::close(sv[0]);
    ::close(sv[1]);
}

TEST(init_handshake_idempotent) {
    // initHandshake() should be a no-op if already initialized
    TestEnv env;

    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock = std::make_unique<TLSSocket>(sv[0], true);
    sock->setNonBlocking(true);
    sock->initHandshake();
    SSL* first = sock->getSSL();
    ASSERT_TRUE(first != nullptr);

    // Second call should be a no-op (same SSL*)
    sock->initHandshake();
    ASSERT_EQ(sock->getSSL(), first);

    ::close(sv[0]);
    ::close(sv[1]);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "AsyncTlsSocket Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
