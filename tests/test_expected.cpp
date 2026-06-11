// OmniNet - std::expected Unit Tests
#include <iostream>
#include <string>

#include "utils/Expected.h"
#include "db/Database.h"
#include "net/Socket.h"

using namespace OmniNet;
using namespace OmniNet::DB;

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

static std::string tmpDbPath() {
    static int counter = 0;
    return "/tmp/test_expected_db_" + std::to_string(getpid()) + "_" +
           std::to_string(++counter) + ".db";
}

// ============================================================================
// Tests
// ============================================================================

TEST(expected_success_path) {
    std::expected<int, SocketError> result = 42;
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 42);
}

TEST(expected_error_path) {
    std::expected<int, SocketError> result =
        std::unexpected<SocketError>{SocketError::ConnectionRefused};
    ASSERT_TRUE(!result.has_value());
    ASSERT_EQ(result.error(), SocketError::ConnectionRefused);
}

TEST(expected_chained_operations) {
    // Chain: get user -> get profile
    std::expected<int, SocketError> userResult = 1;
    if (!userResult) {
        ASSERT_TRUE(false);  // Should not get here
    }

    std::expected<std::string, SocketError> profileResult =
        std::string("profile data");
    if (!profileResult) {
        ASSERT_TRUE(false);
    }

    ASSERT_EQ(*userResult + 0, 1);
    ASSERT_EQ(*profileResult, std::string("profile data"));
}

TEST(expected_void_value) {
    VoidResult<DbError> result{};  // Success (void)
    ASSERT_TRUE(result.has_value());

    VoidResult<DbError> error = std::unexpected<DbError>{DbError::NotOpen};
    ASSERT_TRUE(!error.has_value());
    ASSERT_EQ(error.error(), DbError::NotOpen);
}

TEST(expected_toString_socket) {
    ASSERT_EQ(toString(SocketError::Success), std::string_view("Success"));
    ASSERT_EQ(toString(SocketError::ConnectionRefused),
              std::string_view("Connection refused"));
    ASSERT_EQ(toString(SocketError::InvalidAddress),
              std::string_view("Invalid address"));
}

TEST(expected_toString_db) {
    ASSERT_EQ(toString(DbError::Success), std::string_view("Success"));
    ASSERT_EQ(toString(DbError::Busy), std::string_view("Database busy"));
    ASSERT_EQ(toString(DbError::ConstraintViolation),
              std::string_view("Constraint violation"));
}

// Database::tryOpen tests
TEST(database_tryOpen_success) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto result = db.tryOpen(path);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(db.isOpen());
    std::remove(path.c_str());
}

TEST(database_tryOpen_twice_fails) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto result1 = db.tryOpen(path);
    ASSERT_TRUE(result1.has_value());

    // Second open should fail with AlreadyOpen
    auto result2 = db.tryOpen(path);
    ASSERT_TRUE(!result2.has_value());
    ASSERT_EQ(result2.error(), DbError::AlreadyOpen);
    std::remove(path.c_str());
}

TEST(database_tryOpen_invalid_path) {
    // v0.6.0: tryOpen should map SQLITE_CANTOPEN to DbError::InvalidPath
    // Try to open a path inside a non-existent directory
    Database db;
    auto result = db.tryOpen("/nonexistent/dir/should/not/exist/db.sqlite");
    ASSERT_TRUE(!result.has_value());
    // Should be InvalidPath, not Unknown
    ASSERT_EQ(result.error(), DbError::InvalidPath);
}

TEST(database_tryOpen_error_messages) {
    // Sanity: error.toString() doesn't crash and returns a message
    Database db;
    auto result = db.tryOpen("/nonexistent/dir/db.sqlite");
    ASSERT_TRUE(!result.has_value());
    std::string msg(toString(result.error()));
    ASSERT_TRUE(!msg.empty());
    ASSERT_TRUE(msg != "Success");
}

TEST(database_tryExecute_success) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto openResult = db.tryOpen(path);
    ASSERT_TRUE(openResult.has_value());

    auto result = db.tryExecute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    ASSERT_TRUE(result.has_value());
    std::remove(path.c_str());
}

TEST(database_tryExecute_not_open) {
    Database db;
    auto result = db.tryExecute("SELECT 1");
    ASSERT_TRUE(!result.has_value());
    ASSERT_EQ(result.error(), DbError::NotOpen);
}

TEST(database_tryExecute_constraint_violation) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto openResult = db.tryOpen(path);
    ASSERT_TRUE(openResult.has_value());

    auto r1 = db.tryExecute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT UNIQUE)");
    ASSERT_TRUE(r1.has_value());
    auto r2 = db.tryExecute("INSERT INTO users (id, name) VALUES (1, 'Alice')");
    ASSERT_TRUE(r2.has_value());

    // Insert duplicate - should fail with constraint violation
    auto result = db.tryExecute("INSERT INTO users (id, name) VALUES (2, 'Alice')");
    ASSERT_TRUE(!result.has_value());
    ASSERT_EQ(result.error(), DbError::ConstraintViolation);
    std::remove(path.c_str());
}

TEST(database_tryQueryOne_success) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto r0 = db.tryOpen(path); ASSERT_TRUE(r0.has_value());
    auto r1 = db.tryExecute("CREATE TABLE users (id INTEGER, name TEXT)");
    ASSERT_TRUE(r1.has_value());
    auto r2 = db.tryExecute("INSERT INTO users VALUES (1, 'Alice')");
    ASSERT_TRUE(r2.has_value());

    auto result = db.tryQueryOne("SELECT name FROM users WHERE id = 1");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());  // Has a row
    ASSERT_EQ((*result)->size(), (size_t)1);
    ASSERT_EQ((*result)->at(0), std::string("Alice"));
    std::remove(path.c_str());
}

TEST(database_tryQueryOne_no_results) {
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    auto r0 = db.tryOpen(path); ASSERT_TRUE(r0.has_value());
    auto r1 = db.tryExecute("CREATE TABLE users (id INTEGER, name TEXT)");
    ASSERT_TRUE(r1.has_value());

    auto result = db.tryQueryOne("SELECT name FROM users WHERE id = 999");
    ASSERT_TRUE(result.has_value());  // Query succeeded
    ASSERT_TRUE(!result->has_value());  // But no row
    std::remove(path.c_str());
}

TEST(database_tryQueryOne_not_open) {
    Database db;
    auto result = db.tryQueryOne("SELECT 1");
    ASSERT_TRUE(!result.has_value());
    ASSERT_EQ(result.error(), DbError::NotOpen);
}

TEST(socket_tryConnect_invalid_host) {
    Socket sock;
    auto result = sock.tryConnect("this-host-does-not-exist-12345.invalid", 80);
    ASSERT_TRUE(!result.has_value());
    // Should be HostUnreachable or InvalidAddress
    ASSERT_TRUE(result.error() == SocketError::HostUnreachable ||
                result.error() == SocketError::InvalidAddress);
}

TEST(socket_tryBind_already_bound) {
    // Use a low port that's likely available
    Socket sock1;
    auto result1 = sock1.tryBind("127.0.0.1", 18501);
    if (result1) {
        // Port 18501 is now bound, try to bind another socket
        Socket sock2;
        auto result2 = sock2.tryBind("127.0.0.1", 18501);
        ASSERT_TRUE(!result2.has_value());
        ASSERT_EQ(result2.error(), SocketError::AddressInUse);
    }
}

TEST(back_compat_void_api) {
    // Old void open() API should still work
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    bool threw = false;
    try {
        db.open(path);
    } catch (...) {
        threw = true;
    }
    ASSERT_TRUE(!threw);
    ASSERT_TRUE(db.isOpen());
    std::remove(path.c_str());
}

TEST(back_compat_optional_api) {
    // Old queryOne() returning std::optional should still work
    auto path = tmpDbPath();
    std::remove(path.c_str());

    Database db;
    db.open(path);
    db.execute("CREATE TABLE x (v INTEGER)");
    db.execute("INSERT INTO x VALUES (42)");

    auto result = db.queryOne("SELECT v FROM x");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ((*result)[0], std::string("42"));
    std::remove(path.c_str());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "std::expected Migration Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Tests run via static initializers above

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}
