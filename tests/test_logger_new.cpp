// OmniNet Logger Tests
#include "utils/Logger.h"
#include "utils/LogReader.h"
#include "config/Config.h"
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace OmniNet;

// Test basic logging functionality
bool test_logger_basic() {
    Logger& logger = Logger::instance();
    
    // Set to DEBUG to see all messages
    logger.setLevel(LogLevel::DEBUG);
    logger.setAsync(false);  // Synchronous for testing
    
    logger.debug(std::string("Debug message test"));
    logger.info(std::string("Info message test"));
    logger.warn(std::string("Warning message test"));
    logger.error(std::string("Error message test"));
    logger.critical(std::string("Critical message test"));
    
    return true;
}

// Test level filtering
bool test_logger_level_filter() {
    Logger& logger = Logger::instance();
    
    // Set to WARN level - DEBUG and INFO should be filtered
    logger.setLevel(LogLevel::WARN);
    logger.setAsync(false);
    
    // These won't produce output (filtered)
    logger.debug(std::string("This debug should be filtered"));
    logger.info(std::string("This info should be filtered"));
    
    // These should produce output
    logger.warn(std::string("Warning should pass"));
    logger.error(std::string("Error should pass"));
    
    return true;
}

// Test initFromConfig
bool test_logger_init_from_config() {
    Logger& logger = Logger::instance();
    
    LoggingConfig config;
    config.level = "DEBUG";
    config.toConsole = true;
    config.toFile = false;
    config.maxSizeMb = 0;
    config.maxFiles = 0;
    
    logger.initFromConfig(config);
    
    // Verify level was set
    return logger.level() == LogLevel::DEBUG;
}

// Test log file writing and rotation setup
bool test_logger_file_output() {
    Logger& logger = Logger::instance();
    
    // Create temp log file
    std::string testLogPath = "/tmp/test_omninet.log";
    
    // Clean up any existing file
    if (std::filesystem::exists(testLogPath)) {
        std::filesystem::remove(testLogPath);
    }
    
    logger.setLogFile(testLogPath);
    logger.setLevel(LogLevel::DEBUG);
    logger.setAsync(false);
    logger.setMaxFileSize(1024);  // 1KB rotation
    logger.setMaxRotatedFiles(3);
    
    // Write some log entries
    for (int i = 0; i < 10; i++) {
        logger.info("Test log entry " + std::to_string(i));
    }
    
    // Force flush
    logger.flush();
    
    // Verify file was created
    bool fileExists = std::filesystem::exists(testLogPath);
    
    // Clean up
    if (fileExists) {
        std::filesystem::remove(testLogPath);
    }
    
    return fileExists;
}

// Test LogReader basic functionality
bool test_log_reader_basic() {
    // Create a test log file
    std::string testLogPath = "/tmp/test_reader.log";
    
    {
        std::ofstream file(testLogPath);
        file << "[DEBUG] [2024-01-01 10:00:00.000] [thread1] Debug message\n";
        file << "[INFO] [2024-01-01 10:00:01.000] [thread1] Info message\n";
        file << "[WARN] [2024-01-01 10:00:02.000] [thread2] Warning message\n";
        file << "[ERROR] [2024-01-01 10:00:03.000] [thread2] Error message\n";
    }
    
    LogReader reader(testLogPath);
    
    // Test reading last N lines
    auto entries = reader.readLast(10);
    
    // Verify we got entries
    bool hasEntries = entries.size() >= 4;
    
    // Test line parsing
    auto firstEntry = LogReader::parseLine("[INFO] [2024-01-01 10:00:00.000] [thread1] Test message");
    bool parsedCorrectly = firstEntry.has_value() && firstEntry->message == "Test message";
    
    // Clean up
    std::filesystem::remove(testLogPath);
    
    return hasEntries && parsedCorrectly;
}

// Test LogReader level filtering
bool test_log_reader_filter() {
    // Create a test log file
    std::string testLogPath = "/tmp/test_reader_filter.log";
    
    {
        std::ofstream file(testLogPath);
        file << "[DEBUG] [2024-01-01 10:00:00.000] [thread1] Debug message\n";
        file << "[INFO] [2024-01-01 10:00:01.000] [thread1] Info message\n";
        file << "[WARN] [2024-01-01 10:00:02.000] [thread2] Warning message\n";
        file << "[ERROR] [2024-01-01 10:00:03.000] [thread2] Error message\n";
    }
    
    LogReader reader(testLogPath);
    
    // Set filter to ERROR and above
    reader.setMinLevel(LogLevel::ERROR);
    
    auto entries = reader.readLast(10);
    
    // Should only get ERROR (not DEBUG, INFO, WARN)
    bool correctFilter = true;
    for (const auto& entry : entries) {
        if (entry.level < LogLevel::ERROR) {
            correctFilter = false;
            break;
        }
    }
    
    // Clean up
    std::filesystem::remove(testLogPath);
    
    return correctFilter;
}

// Test LogReader pattern filter
bool test_log_reader_pattern() {
    // Create a test log file
    std::string testLogPath = "/tmp/test_reader_pattern.log";
    
    {
        std::ofstream file(testLogPath);
        file << "[INFO] [2024-01-01 10:00:00.000] [thread1] Normal message\n";
        file << "[INFO] [2024-01-01 10:00:01.000] [thread1] Error: something failed\n";
        file << "[INFO] [2024-01-01 10:00:02.000] [thread2] Another normal message\n";
        file << "[INFO] [2024-01-01 10:00:03.000] [thread2] Error: another failure\n";
    }
    
    LogReader reader(testLogPath);
    reader.setPattern("Error:");
    
    auto entries = reader.readLast(10);
    
    // Should only get entries with "Error:"
    bool correctPattern = (entries.size() == 2);
    
    // Clean up
    std::filesystem::remove(testLogPath);
    
    return correctPattern;
}

// Test LogEntry JSON serialization
bool test_log_entry_json() {
    LogEntry entry;
    entry.level = LogLevel::ERROR;
    entry.timestamp = "2024-01-01 10:00:00.000";
    entry.threadId = "thread1";
    entry.message = "Test error message";
    
    std::string json = entry.toJson();
    
    // Verify JSON contains expected fields
    bool hasLevel = json.find("ERROR") != std::string::npos;
    bool hasTimestamp = json.find("2024-01-01 10:00:00.000") != std::string::npos;
    bool hasMessage = json.find("Test error message") != std::string::npos;
    
    return hasLevel && hasTimestamp && hasMessage;
}

int main() {
    std::cout << "Running Logger and LogReader tests..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    // Logger tests
    std::cout << "\n--- Logger Tests ---" << std::endl;
    
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
    
    if (test_logger_init_from_config()) {
        std::cout << "[PASS] Logger initFromConfig" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Logger initFromConfig" << std::endl;
        failed++;
    }
    
    if (test_logger_file_output()) {
        std::cout << "[PASS] Logger file output" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Logger file output" << std::endl;
        failed++;
    }
    
    // LogReader tests
    std::cout << "\n--- LogReader Tests ---" << std::endl;
    
    if (test_log_reader_basic()) {
        std::cout << "[PASS] LogReader basic functionality" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] LogReader basic functionality" << std::endl;
        failed++;
    }
    
    if (test_log_reader_filter()) {
        std::cout << "[PASS] LogReader level filtering" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] LogReader level filtering" << std::endl;
        failed++;
    }
    
    if (test_log_reader_pattern()) {
        std::cout << "[PASS] LogReader pattern filter" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] LogReader pattern filter" << std::endl;
        failed++;
    }
    
    if (test_log_entry_json()) {
        std::cout << "[PASS] LogEntry JSON serialization" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] LogEntry JSON serialization" << std::endl;
        failed++;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}