// OmniNet - Public Test Framework Header
// 
// This header provides a test framework for users of the OmniNet library.
// It defines test macros that will NOT conflict with user code.
//
// Usage:
//   #include <OmniNet/Test.h>
//   
//   // Register a test function
//   OMNINET_TEST(my_test) {
//       OMNINET_ASSERT_TRUE(some_condition);
//       OMNINET_ASSERT_EQ(value1, value2);
//   }
//   
//   // Run tests
//   int main() {
//       return OMNINET_RUN_TESTS();
//   }

#ifndef OMNINET_TEST_H
#define OMNINET_TEST_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// ============================================================================
// Test Registry
// ============================================================================

namespace OmniNet::Test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class Registry {
public:
    static Registry& instance() {
        static Registry instance;
        return instance;
    }
    
    void add(const std::string& name, std::function<void()> func) {
        m_tests.push_back({name, func});
    }
    
    const std::vector<TestCase>& tests() const { return m_tests; }
    
    void reset() { m_tests.clear(); }
    
    int runAll() {
        int passed = 0;
        int failed = 0;
        int skipped = 0;
        
        std::cout << "========================================\n";
        std::cout << "OmniNet Test Suite\n";
        std::cout << "========================================\n\n";
        
        for (const auto& test : m_tests) {
            std::cout << "  " << test.name << "... ";
            try {
                test.func();
                std::cout << "PASS\n";
                passed++;
            } catch (const std::exception& e) {
                std::cout << "FAIL (" << e.what() << ")\n";
                failed++;
            } catch (...) {
                std::cout << "FAIL\n";
                failed++;
            }
        }
        
        std::cout << "\n========================================\n";
        std::cout << "Results: " << passed << " passed, " << failed << " failed, " << skipped << " skipped\n";
        std::cout << "========================================\n";
        
        return failed > 0 ? 1 : 0;
    }

private:
    std::vector<TestCase> m_tests;
};

} // namespace OmniNet::Test

// ============================================================================
// Test Macros
// ============================================================================

// Define a test function
// Usage: OMNINET_TEST(my_test_name) { ... test code ... }
#define OMNINET_TEST(name) \
    void test_##name(); \
    static bool name##_registered_ = []() { \
        OmniNet::Test::Registry::instance().add(#name, test_##name); \
        return true; \
    }(); \
    void test_##name()

// Register a test (for manual registration)
#define OMNINET_REGISTER_TEST(name, func) \
    static bool name##_registered_ = []() { \
        OmniNet::Test::Registry::instance().add(#name, func); \
        return true; \
    }()

// Run all registered tests
#define OMNINET_RUN_TESTS() \
    OmniNet::Test::Registry::instance().runAll()

// ============================================================================
// Assertion Macros
// ============================================================================

// Assert that condition is true
#define OMNINET_ASSERT_TRUE(cond) \
    do { if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); } while(0)

// Assert that condition is false
#define OMNINET_ASSERT_FALSE(cond) \
    do { if (cond) throw std::runtime_error("Assertion failed: NOT " #cond); } while(0)

// Assert two values are equal
#define OMNINET_ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a != _b) { \
            throw std::runtime_error("Assertion failed: " #a " (" + std::to_string(_a) + ") != " #b); \
        } \
    } while(0)

// Assert two values are not equal
#define OMNINET_ASSERT_NE(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a == _b) { \
            throw std::runtime_error("Assertion failed: " #a " == " #b); \
        } \
    } while(0)

// Assert a > b
#define OMNINET_ASSERT_GT(a, b) \
    do { \
        if ((a) <= (b)) { \
            throw std::runtime_error("Assertion failed: " #a " <= " #b); \
        } \
    } while(0)

// Assert a >= b
#define OMNINET_ASSERT_GE(a, b) \
    do { \
        if ((a) < (b)) { \
            throw std::runtime_error("Assertion failed: " #a " < " #b); \
        } \
    } while(0)

// Assert a < b
#define OMNINET_ASSERT_LT(a, b) \
    do { \
        if ((a) >= (b)) { \
            throw std::runtime_error("Assertion failed: " #a " >= " #b); \
        } \
    } while(0)

// Assert a <= b
#define OMNINET_ASSERT_LE(a, b) \
    do { \
        if ((a) > (b)) { \
            throw std::runtime_error("Assertion failed: " #a " > " #b); \
        } \
    } while(0)

// Assert string contains substring
#define OMNINET_ASSERT_STR_CONTAINS(str, substr) \
    do { \
        if ((str).find(substr) == std::string::npos) { \
            throw std::runtime_error("Assertion failed: '" + str + "' does not contain '" + substr + "'"); \
        } \
    } while(0)

// Assert string does not contain substring
#define OMNINET_ASSERT_STR_NOT_CONTAINS(str, substr) \
    do { \
        if ((str).find(substr) != std::string::npos) { \
            throw std::runtime_error("Assertion failed: '" + str + "' contains '" + substr + "'"); \
        } \
    } while(0)

// Assert throws an exception
#define OMNINET_ASSERT_THROWS(expr, exception_type) \
    do { \
        bool _threw = false; \
        try { \
            expr; \
        } catch (const exception_type&) { \
            _threw = true; \
        } catch (...) { \
            _threw = false; \
        } \
        if (!_threw) { \
            throw std::runtime_error("Assertion failed: " #expr " did not throw " #exception_type); \
        } \
    } while(0)

// Assert does not throw
#define OMNINET_ASSERT_NO_THROW(expr) \
    do { \
        try { \
            expr; \
        } catch (...) { \
            throw std::runtime_error("Assertion failed: " #expr " threw an exception"); \
        } \
    } while(0)

// Fail test with message
#define OMNINET_FAIL(msg) \
    do { throw std::runtime_error("FAIL: " + std::string(msg)); } while(0)

// ============================================================================
// Expectation Macros (non-fatal failures)
// ============================================================================

// Expect (doesn't abort test, just counts)
#define OMNINET_EXPECT_TRUE(cond) \
    do { if (!(cond)) { std::cerr << "EXPECT FAILED: " #cond << std::endl; } } while(0)

#define OMNINET_EXPECT_FALSE(cond) \
    do { if (cond) { std::cerr << "EXPECT FAILED: NOT " #cond << std::endl; } } while(0)

#define OMNINET_EXPECT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "EXPECT FAILED: " << #a << " (" << (a) << ") != " << #b << std::endl; \
        } \
    } while(0)

#endif // OMNINET_TEST_H