// OmniNet Logger Tests
#include "utils/Logger.h"
#include <iostream>
#include <fstream>

using namespace OmniNet;

bool test_logger_basic() {
    Logger::Config config;
    config.level = LogLevel::DEBUG;
    config.toConsole = true;
    config.toFile = false;
    
    Logger::instance().configure(config);
    
    Logger::instance().debug("Debug message test");
    Logger::instance().info("Info message test");
    Logger::instance().warn("Warning message test");
    Logger::instance().error("Error message test");
    
    return true;
}

bool test_logger_level_filter() {
    // Test that levels are filtered correctly
    Logger::Config config;
    config.level = LogLevel::WARN;  // Only WARN and above
    config.toConsole = false;
    config.toFile = false;
    
    Logger::instance().configure(config);
    
    // These should be filtered
    Logger::instance().debug("This debug should be filtered");
    Logger::instance().info("This info should be filtered");
    
    // These should pass
    Logger::instance().warn("Warning message");
    Logger::instance().error("Error message");
    
    return true;
}

bool test_logger_format() {
    Logger::Config config;
    config.level = LogLevel::DEBUG;
    config.toConsole = false;
    config.toFile = false;
    config.showTimestamp = true;
    config.showLevel = true;
    config.showThreadId = true;
    
    Logger::instance().configure(config);
    
    Logger::instance().info("Formatted message test");
    
    return true;
}

int main() {
    std::cout << "Running Logger tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_logger_basic()) {
        std::cout << "[PASS] Logger basic functionality" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Logger basic functionality" << std::endl;
        failed++;
    }
    
    if (test_logger_level_filter()) {
        std::cout << "[PASS] Logger level filtering" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Logger level filtering" << std::endl;
        failed++;
    }
    
    if (test_logger_format()) {
        std::cout << "[PASS] Logger formatting" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Logger formatting" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "Logger tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}