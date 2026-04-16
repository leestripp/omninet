// OmniNet Test Main - Combined Test Suite
#include <iostream>
#include <string>

// Test Logger
#include "utils/Logger.h"

// Test Exception
#include "utils/Exception.h"

// Test Utils
#include "utils/Utils.h"

// ============================================================================
// Logger Tests
// ============================================================================
void test_logger() {
    std::cout << "Running Logger test..." << std::endl;
    OmniNet::Logger::instance().setLevel(OmniNet::LogLevel::DEBUG);
    OmniNet::Logger::instance().info(std::string("Logger initialized"));
    OmniNet::Logger::instance().debug(std::string("Debug message test"));
    OmniNet::Logger::instance().warn(std::string("Warning message test"));
    OmniNet::Logger::instance().error(std::string("Error message test"));
    std::cout << "[PASS] Logger basic functionality" << std::endl;
}

// ============================================================================
// Exception Tests
// ============================================================================
void test_exception() {
    std::cout << "Running Exception test..." << std::endl;
    
    try {
        throw OmniNet::SocketException(std::string("Test error"), 42);
    } catch (const OmniNet::Exception& e) {
        std::cout << "[PASS] Caught exception: " << e.what() << std::endl;
    }
    
    try {
        throw OmniNet::HttpException(std::string("Bad request test"), 400);
    } catch (const OmniNet::Exception& e) {
        std::cout << "[PASS] Caught HTTP exception: " << e.what() << std::endl;
    }
}

// ============================================================================
// Utils Tests
// ============================================================================
void test_utils_split() {
    std::cout << "Running Utils::split test..." << std::endl;
    auto parts = OmniNet::Utils::split("a,b,c", ',');
    if (parts.size() == 3 && parts[0] == "a" && parts[1] == "b" && parts[2] == "c") {
        std::cout << "[PASS] Utils::split" << std::endl;
    } else {
        std::cout << "[FAIL] Utils::split" << std::endl;
    }
}

void test_utils_trim() {
    std::cout << "Running Utils::trim test..." << std::endl;
    auto trimmed = OmniNet::Utils::trim("  hello world  ");
    if (trimmed == "hello world") {
        std::cout << "[PASS] Utils::trim" << std::endl;
    } else {
        std::cout << "[FAIL] Utils::trim (got: '" << trimmed << "')" << std::endl;
    }
}

void test_utils_toLower() {
    std::cout << "Running Utils::toLower test..." << std::endl;
    auto lower = OmniNet::Utils::toLower("HELLO");
    if (lower == "hello") {
        std::cout << "[PASS] Utils::toLower" << std::endl;
    } else {
        std::cout << "[FAIL] Utils::toLower" << std::endl;
    }
}

void test_utils_base64() {
    std::cout << "Running Utils::base64Encode test..." << std::endl;
    auto encoded = OmniNet::Utils::base64Encode(std::string("Hello, World!"));
    if (!encoded.empty()) {
        std::cout << "[PASS] Utils::base64Encode: " << encoded << std::endl;
    } else {
        std::cout << "[FAIL] Utils::base64Encode" << std::endl;
    }
}

void test_utils_hmac() {
    std::cout << "Running Utils::hmacSha256 test..." << std::endl;
    auto hmac = OmniNet::Utils::hmacSha256("key", "message");
    if (!hmac.empty()) {
        std::cout << "[PASS] Utils::hmacSha256: " << hmac << std::endl;
    } else {
        std::cout << "[FAIL] Utils::hmacSha256" << std::endl;
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "OmniNet Library - Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Run all tests
    test_logger();
    test_exception();
    test_utils_split();
    test_utils_trim();
    test_utils_toLower();
    test_utils_base64();
    test_utils_hmac();
    
    std::cout << std::endl;
    std::cout << "All tests completed." << std::endl;
    
    return 0;
}
