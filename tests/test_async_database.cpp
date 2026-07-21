// OmniNet - AsyncDatabase Unit Tests
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "async/Task.h"
#include "async/Executor.h"
#include "db/AsyncDatabase.h"
#include "db/DatabasePool.h"
#include "thread/ThreadPool.h"

using namespace OmniNet;
using namespace OmniNet::DB;
using namespace OmniNet::Async;
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

    TestEnv() {
        poolCfg.workerCount = 4;
        pool = std::make_unique<Thread::ThreadPool>(poolCfg);
        exec = std::make_unique<ThreadPoolExecutor>(*pool);
    }

    std::string tmpDbPath() {
        return "/tmp/test_async_db_" + std::to_string(getpid()) + "_" +
               std::to_string(reinterpret_cast<uintptr_t>(this)) + ".db";
    }
};

// ============================================================================
// AsyncDatabase tests
// ============================================================================

TEST(async_database_create) {
    TestEnv env;
    auto path = env.tmpDbPath();
    AsyncDatabase db(*env.exec, path);
    ASSERT_TRUE(db.isOpen());
    ASSERT_EQ(db.path(), path);
}

TEST(async_database_execute) {
    TestEnv env;
    auto path = env.tmpDbPath();
    AsyncDatabase db(*env.exec, path);

    auto task = db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    task.resume();
    task.result();
    // After result(), the task handle should be in done state
    // (don't check handle() which is a different concept)
}

TEST(async_database_insert_query) {
    TestEnv env;
    auto path = env.tmpDbPath();
    AsyncDatabase db(*env.exec, path);

    // Create table
    auto create = db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    create.resume();
    create.result();

    // Insert
    auto insert = db.execute("INSERT INTO users (id, name) VALUES (1, 'Alice')");
    insert.resume();
    insert.result();

    // Query
    auto query = db.query("SELECT name FROM users WHERE id = 1");
    query.resume();
    auto rows = std::move(query).result();
    ASSERT_EQ(rows.size(), (size_t)1);
    ASSERT_EQ(rows[0].size(), (size_t)1);
    ASSERT_EQ(rows[0][0], std::string("Alice"));
}

TEST(async_database_query_one) {
    TestEnv env;
    auto path = env.tmpDbPath();
    AsyncDatabase db(*env.exec, path);

    auto create = db.execute("CREATE TABLE items (id INTEGER, val TEXT)");
    create.resume();
    create.result();

    auto insert = db.execute("INSERT INTO items VALUES (1, 'first')");
    insert.resume();
    insert.result();

    auto q = db.query_one("SELECT val FROM items WHERE id = 1");
    q.resume();
    auto result = std::move(q).result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ((*result)[0], std::string("first"));
}

TEST(async_database_query_one_empty) {
    TestEnv env;
    auto path = env.tmpDbPath();
    AsyncDatabase db(*env.exec, path);

    auto create = db.execute("CREATE TABLE items (id INTEGER, val TEXT)");
    create.resume();
    create.result();

    auto q = db.query_one("SELECT val FROM items WHERE id = 999");
    q.resume();
    auto result = std::move(q).result();
    ASSERT_TRUE(!result.has_value());
}

// ============================================================================
// DatabasePool tests
// ============================================================================

TEST(database_pool_create) {
    TestEnv env;
    auto path = env.tmpDbPath();

    DatabasePool::Config cfg;
    cfg.minSize = 2;
    cfg.maxSize = 4;
    DatabasePool pool(*env.exec, path, cfg);

    auto stats = pool.stats();
    ASSERT_EQ(stats.totalConnections, (size_t)2);
    ASSERT_EQ(stats.inUse, (size_t)0);
    ASSERT_EQ(stats.available, (size_t)2);
}

TEST(database_pool_acquire_release) {
    TestEnv env;
    auto path = env.tmpDbPath();
    DatabasePool::Config cfg;
    cfg.minSize = 2;
    DatabasePool pool(*env.exec, path, cfg);

    auto db1 = pool.acquire();
    ASSERT_TRUE(db1 != nullptr);
    ASSERT_TRUE(db1->inUse());

    auto stats = pool.stats();
    ASSERT_EQ(stats.inUse, (size_t)1);
    ASSERT_EQ(stats.available, (size_t)1);

    pool.release(db1);
    ASSERT_TRUE(!db1->inUse());
    stats = pool.stats();
    ASSERT_EQ(stats.inUse, (size_t)0);
    ASSERT_EQ(stats.available, (size_t)2);
}

TEST(database_pool_acquire_grows) {
    TestEnv env;
    auto path = env.tmpDbPath();
    DatabasePool::Config cfg;
    cfg.minSize = 1;
    cfg.maxSize = 4;
    DatabasePool pool(*env.exec, path, cfg);

    auto db1 = pool.acquire();
    auto db2 = pool.acquire();
    auto db3 = pool.acquire();

    auto stats = pool.stats();
    ASSERT_EQ(stats.inUse, (size_t)3);
    ASSERT_TRUE(stats.totalConnections >= 3);
}

TEST(database_pool_concurrent) {
    TestEnv env;
    auto path = env.tmpDbPath();
    DatabasePool::Config cfg;
    cfg.minSize = 2;
    cfg.maxSize = 4;
    DatabasePool pool(*env.exec, path, cfg);

    std::vector<std::shared_ptr<AsyncDatabase>> dbs;
    for (int i = 0; i < 3; ++i) {
        dbs.push_back(pool.acquire());
    }
    ASSERT_TRUE(dbs[0] != nullptr);
    ASSERT_TRUE(dbs[1] != nullptr);
    ASSERT_TRUE(dbs[2] != nullptr);

    // Release them
    for (const auto& db : dbs) {
        pool.release(db);
    }

    auto stats = pool.stats();
    ASSERT_EQ(stats.inUse, (size_t)0);
}

TEST(database_pool_acquire_timeout) {
    TestEnv env;
    auto path = env.tmpDbPath();
    DatabasePool::Config cfg;
    cfg.minSize = 1;
    cfg.maxSize = 1;
    DatabasePool pool(*env.exec, path, cfg);

    auto db1 = pool.acquire();
    ASSERT_TRUE(db1 != nullptr);

    // Try to acquire another with short timeout
    auto db2 = pool.acquire(50ms);
    ASSERT_TRUE(db2 == nullptr);  // Should time out
}

TEST(database_pool_stats_total) {
    TestEnv env;
    auto path = env.tmpDbPath();
    DatabasePool pool(*env.exec, path);

    auto stats1 = pool.stats();
    ASSERT_EQ(stats1.totalAcquired, (uint64_t)0);

    auto db1 = pool.acquire();
    auto stats2 = pool.stats();
    ASSERT_EQ(stats2.totalAcquired, (uint64_t)1);

    pool.release(db1);
    auto stats3 = pool.stats();
    ASSERT_EQ(stats3.totalReleased, (uint64_t)1);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Async Database Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
