// OmniNet - AsyncFile Unit Tests
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "io/AsyncFile.h"
#include "async/Task.h"
#include "async/Executor.h"
#include "thread/ThreadPool.h"

using namespace OmniNet;
using namespace OmniNet::IO;
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
        poolCfg.workerCount = 2;
        pool = std::make_unique<Thread::ThreadPool>(poolCfg);
        exec = std::make_unique<ThreadPoolExecutor>(*pool);
    }
};

static std::string makePath(const std::string& name) {
    return "/tmp/test_async_file_" + std::to_string(getpid()) + "_" + name;
}

static void writeFileSync(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(content.data(), content.size());
}

// ============================================================================
// Tests
// ============================================================================

TEST(construct_default) {
    TestEnv env;
    AsyncFile file(*env.exec);
    ASSERT_TRUE(!file.isOpen());
    ASSERT_EQ(file.fd(), -1);
}

TEST(open_and_close) {
    TestEnv env;
    auto path = makePath("open_close.txt");
    writeFileSync(path, "test");

    AsyncFile file(*env.exec);
    auto openTask = file.open(path);
    openTask.resume();
    openTask.result();

    ASSERT_TRUE(file.isOpen());
    ASSERT_TRUE(file.fd() >= 0);
    ASSERT_EQ(file.path(), path);

    auto closeTask = file.close();
    closeTask.resume();
    closeTask.result();

    ASSERT_TRUE(!file.isOpen());
    std::remove(path.c_str());
}

TEST(open_nonexistent) {
    TestEnv env;
    AsyncFile file(*env.exec);
    auto openTask = file.open("/tmp/nonexistent_file_xyz_999.txt");
    openTask.resume();
    openTask.result();

    ASSERT_TRUE(!file.isOpen());
    ASSERT_TRUE(!file.lastError().empty());
}

TEST(read_all) {
    TestEnv env;
    auto path = makePath("read_all.txt");
    std::string content = "Hello, AsyncFile! This is a test of read_all().";
    writeFileSync(path, content);

    AsyncFile file(*env.exec);
    file.open(path).resume();

    auto task = file.read_all();
    task.resume();
    std::vector<uint8_t> data = std::move(task).result();

    ASSERT_EQ(data.size(), content.size());
    ASSERT_EQ(std::string(reinterpret_cast<char*>(data.data()), data.size()),
              content);

    file.close().resume();
    std::remove(path.c_str());
}

TEST(read_all_empty_file) {
    TestEnv env;
    auto path = makePath("empty.txt");
    writeFileSync(path, "");

    AsyncFile file(*env.exec);
    file.open(path).resume();

    auto task = file.read_all();
    task.resume();
    std::vector<uint8_t> data = std::move(task).result();

    ASSERT_EQ(data.size(), (size_t)0);

    file.close().resume();
    std::remove(path.c_str());
}

TEST(read_range) {
    TestEnv env;
    auto path = makePath("read_range.txt");
    std::string content = "0123456789ABCDEF";
    writeFileSync(path, content);

    AsyncFile file(*env.exec);
    file.open(path).resume();

    // Read bytes 5-10
    auto task = file.read_range(5, 6);
    task.resume();
    std::vector<uint8_t> data = std::move(task).result();

    ASSERT_EQ(data.size(), (size_t)6);
    ASSERT_EQ(std::string(reinterpret_cast<char*>(data.data()), data.size()),
              std::string("56789A"));

    file.close().resume();
    std::remove(path.c_str());
}

TEST(read_range_beyond_eof) {
    TestEnv env;
    auto path = makePath("short.txt");
    std::string content = "hi";
    writeFileSync(path, content);

    AsyncFile file(*env.exec);
    file.open(path).resume();

    // Try to read 100 bytes from a 2-byte file
    auto task = file.read_range(0, 100);
    task.resume();
    std::vector<uint8_t> data = std::move(task).result();

    // Should only get 2 bytes (until EOF)
    ASSERT_EQ(data.size(), (size_t)2);
    ASSERT_EQ(std::string(reinterpret_cast<char*>(data.data()), data.size()),
              content);

    file.close().resume();
    std::remove(path.c_str());
}

TEST(write_and_read) {
    TestEnv env;
    auto path = makePath("write.txt");

    {
        AsyncFile file(*env.exec);
        auto openTask = file.open(path, "w");
        openTask.resume();
        openTask.result();
        ASSERT_TRUE(file.isOpen());

        std::string data = "Written content";
        auto writeTask = file.write(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()), data.size()));
        writeTask.resume();
        size_t written = writeTask.result();
        ASSERT_EQ(written, data.size());

        auto closeTask = file.close();
        closeTask.resume();
    }

    // Verify by reading synchronously
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    ASSERT_EQ(content, std::string("Written content"));

    std::remove(path.c_str());
}

TEST(size_method) {
    TestEnv env;
    auto path = makePath("size.txt");
    writeFileSync(path, "12345");

    AsyncFile file(*env.exec);
    file.open(path).resume();

    ASSERT_EQ(file.size(), (size_t)5);

    file.close().resume();
    std::remove(path.c_str());
}

TEST(destructor_closes_file) {
    TestEnv env;
    auto path = makePath("destructor.txt");
    writeFileSync(path, "data");

    {
        AsyncFile file(*env.exec);
        file.open(path).resume();
        ASSERT_TRUE(file.isOpen());
        // Don't explicitly close - let destructor handle it
    }

    // After destruction, file should be closed but path should still exist
    ASSERT_EQ(std::ifstream(path).good(), true);
    std::remove(path.c_str());
}

TEST(move_construct) {
    TestEnv env;
    auto path = makePath("move.txt");
    writeFileSync(path, "data");

    AsyncFile a(*env.exec);
    a.open(path).resume();

    AsyncFile b = std::move(a);
    ASSERT_TRUE(b.isOpen());
    ASSERT_TRUE(!a.isOpen());  // a's fd transferred

    b.close().resume();
    std::remove(path.c_str());
}

TEST(large_file) {
    TestEnv env;
    auto path = makePath("large.txt");

    // Create a 1MB file with known pattern
    {
        std::ofstream f(path, std::ios::binary);
        std::string chunk(1024, 'X');
        for (int i = 0; i < 1024; ++i) {
            f.write(chunk.data(), chunk.size());
        }
    }

    AsyncFile file(*env.exec);
    file.open(path).resume();
    ASSERT_EQ(file.size(), (size_t)(1024 * 1024));

    auto task = file.read_all();
    task.resume();
    std::vector<uint8_t> data = std::move(task).result();

    ASSERT_EQ(data.size(), (size_t)(1024 * 1024));
    // Verify content
    for (size_t i = 0; i < data.size(); i += 1024) {
        ASSERT_EQ(data[i], (uint8_t)'X');
    }

    file.close().resume();
    std::remove(path.c_str());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "AsyncFile Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
