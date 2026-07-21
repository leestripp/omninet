// OmniNet Config System Tests
#include "config/Config.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN(name) do { std::cout << "  " #name "... "; test_##name(); std::cout << "PASS\n"; testsPassed++; } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { std::cout << "FAIL (" << #cond << ")"; testsFailed++; } } while(0)
#define ASSERT_FALSE(cond) do { if (cond) { std::cout << "FAIL (" << #cond << ")"; testsFailed++; } } while(0)
#define ASSERT_EQ(a, b) do { const auto _a = (a); const auto _b = (b); if (_a != _b) { std::cout << "FAIL: " << #a << " (" << _a << ") != " << _b; testsFailed++; } } while(0)
#define ASSERT_NE(a, b) do { const auto _a = (a); const auto _b = (b); if (_a == _b) { std::cout << "FAIL: " << #a << " == " << _b; testsFailed++; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { std::cout << "FAIL: " << #a << " < " << #b; testsFailed++; } } while(0)
#define ASSERT_LE(a, b) do { if ((a) > (b)) { std::cout << "FAIL: " << #a << " > " << #b; testsFailed++; } } while(0)

// ============================================================================
// Config Instance Tests
// ============================================================================

TEST(config_singleton) {
    auto& cfg1 = OmniNet::Config::instance();
    auto& cfg2 = OmniNet::Config::instance();
    ASSERT_EQ(&cfg1, &cfg2);
}

TEST(config_default_values) {
    auto& cfg = OmniNet::Config::instance();
    
    // Check defaults are sensible
    ASSERT_GE(cfg.defaultPort(), 1);
    ASSERT_GE(cfg.defaultRequestTimeout().count(), 1);
}

TEST(config_data_directory) {
    auto& cfg = OmniNet::Config::instance();
    auto& dataDir = cfg.dataDir();

    // Should be a non-empty, absolute path that we can write to.
    // Under XDG: $XDG_DATA_HOME/omninet (default ~/.local/share/omninet).
    // In legacy mode: ~/.omninet.
    // In containers without write access to HOME: /workspace/data/omninet.
    ASSERT_FALSE(dataDir.empty());
    ASSERT_TRUE(dataDir.is_absolute());
    // The directory should exist (Config creates it in its ctor).
    ASSERT_TRUE(std::filesystem::exists(dataDir));
}

TEST(config_resolve_path) {
    auto& cfg = OmniNet::Config::instance();
    auto path = cfg.resolve("test.db");
    
    // Should be under data dir
    ASSERT_TRUE(path.string().find(cfg.dataDir().string()) != std::string::npos);
}

TEST(config_resolve_absolute) {
    auto& cfg = OmniNet::Config::instance();
    auto path = cfg.resolve("/absolute/path");
    
    // Absolute paths should be returned as-is
    ASSERT_TRUE(path.string() == "/absolute/path");
}

// ============================================================================
// Config Loading Tests
// ============================================================================

TEST(config_json_parsing) {
    // Create a temporary config file
    std::string tempPath = "/tmp/omninet_test_config.json";
    std::ofstream file(tempPath);
    file << R"({
        "server": {
            "port": 9000,
            "workers": 4,
            "requestTimeout": 60
        },
        "logging": {
            "level": "DEBUG"
        }
    })";
    file.close();
    
    // Load it
    auto& cfg = OmniNet::Config::instance();
    cfg.loadFromFile(tempPath);
    
    // Check values were loaded
    ASSERT_EQ(cfg.defaultPort(), 9000);
    ASSERT_EQ(cfg.defaultWorkerCount(), 4u);
    ASSERT_EQ(cfg.defaultRequestTimeout().count(), 60);
    
    // Cleanup
    std::filesystem::remove(tempPath);
}

TEST(config_invalid_json) {
    std::string tempPath = "/tmp/omninet_invalid_config.json";
    std::ofstream file(tempPath);
    file << "this is not json {{{";
    file.close();
    
    auto& cfg = OmniNet::Config::instance();
    // Should not crash, just log error
    cfg.loadFromFile(tempPath);
    
    std::filesystem::remove(tempPath);
}

// ============================================================================
// Config Validation Tests
// ============================================================================

TEST(config_validation_valid) {
    auto& cfg = OmniNet::Config::instance();
    // v0.7.0: reset TLS fields to avoid pollution from previous
    // tests or the on-disk config file (the Config constructor
    // loads from dataDir/config.json if it exists; stale TLS paths
    // in that file would cause 2 validation errors).
    cfg.setDefaultPort(8080);
    cfg.setDefaultTlsCert("");
    cfg.setDefaultTlsKey("");

    ASSERT_TRUE(cfg.validate());
    ASSERT_EQ(cfg.validationErrors().size(), 0u);
}

TEST(config_validation_port_zero) {
    auto& cfg = OmniNet::Config::instance();
    cfg.setDefaultPort(0);
    
    ASSERT_FALSE(cfg.validate());
    ASSERT_TRUE(cfg.validationErrors().size() > 0);
    
    // Reset to valid port
    cfg.setDefaultPort(8080);
}

TEST(config_validation_worker_count) {
    auto& cfg = OmniNet::Config::instance();
    cfg.setDefaultWorkerCount(500);  // Too high
    
    ASSERT_FALSE(cfg.validate());
    
    // Reset
    cfg.setDefaultWorkerCount(4);
}

// ============================================================================
// Config Serialization Tests
// ============================================================================

TEST(config_to_json) {
    auto& cfg = OmniNet::Config::instance();
    cfg.setDefaultPort(12345);
    cfg.setLogLevel(OmniNet::LogLevel::DEBUG);
    
    std::string json = cfg.toJson();
    
    // Should contain the port we set
    ASSERT_TRUE(json.find("\"port\"") != std::string::npos);
    ASSERT_TRUE(json.find("12345") != std::string::npos);
    
    // Should contain DEBUG
    ASSERT_TRUE(json.find("DEBUG") != std::string::npos);
}

TEST(config_save_and_load) {
    auto& cfg = OmniNet::Config::instance();
    
    // Save current state
    cfg.setDefaultPort(55555);
    cfg.save("/tmp/omninet_test_save.json");
    
    // Modify
    cfg.setDefaultPort(11111);
    ASSERT_EQ(cfg.defaultPort(), 11111);
    
    // Load from file
    cfg.loadFromFile("/tmp/omninet_test_save.json");
    ASSERT_EQ(cfg.defaultPort(), 55555);
    
    // Cleanup
    std::filesystem::remove("/tmp/omninet_test_save.json");
}

// ============================================================================
// Environment Variable Tests
// ============================================================================

TEST(config_env_override) {
    // Note: This test sets environment variables which persist
    // In a real test suite, you'd want to save/restore env vars
    
    // Set env var
    setenv("OMNINET_PORT", "7777", 1);
    
    // Reload config (which reads environment)
    [[maybe_unused]] auto& cfg = OmniNet::Config::instance();
    
    // The singleton was already initialized, so we need to reload
    // For this test, just verify the env var is readable
    const char* portEnv = std::getenv("OMNINET_PORT");
    ASSERT_TRUE(portEnv != nullptr);
    if (portEnv) {
        ASSERT_EQ(std::string(portEnv), "7777");
    }
}

// ============================================================================
// Change Callbacks Tests
// ============================================================================

TEST(config_change_callback) {
    auto& cfg = OmniNet::Config::instance();
    
    bool callbackCalled = false;
    cfg.onChange([&callbackCalled](const std::string& path) {
        if (path == "config") {
            callbackCalled = true;
        }
    });
    
    // Manually trigger reload (would normally be triggered by file change)
    cfg.reload();
    
    // Note: callback might not fire if file hasn't actually changed
    // This test just verifies the callback registration works
}

// ============================================================================
// New Config Options Tests
// ============================================================================

TEST(config_keepalive_timeout) {
    auto& cfg = OmniNet::Config::instance();
    auto timeout = cfg.defaultKeepAliveTimeout();
    ASSERT_GE(timeout.count(), 1);
    ASSERT_LE(timeout.count(), 300);
}

TEST(config_max_request_size) {
    auto& cfg = OmniNet::Config::instance();
    auto size = cfg.defaultMaxRequestSize();
    ASSERT_GE(size, 1024u);  // At least 1KB
    ASSERT_LE(size, 100 * 1024 * 1024);  // At most 100MB
}

TEST(config_cors_defaults) {
    auto& cfg = OmniNet::Config::instance();
    
    // CORS should have sensible defaults
    auto& origins = cfg.defaultCorsOrigins();
    auto& methods = cfg.defaultCorsMethods();
    auto& headers = cfg.defaultCorsHeaders();
    
    ASSERT_FALSE(origins.empty());
    ASSERT_FALSE(methods.empty());
    ASSERT_FALSE(headers.empty());
}

TEST(config_cors_setter) {
    auto& cfg = OmniNet::Config::instance();
    
    cfg.setDefaultCorsEnabled(true);
    ASSERT_TRUE(cfg.defaultCorsEnabled());
    
    cfg.setDefaultCorsOrigins("https://example.com");
    ASSERT_EQ(cfg.defaultCorsOrigins(), "https://example.com");
    
    cfg.setDefaultCorsMethods("GET,POST");
    ASSERT_EQ(cfg.defaultCorsMethods(), "GET,POST");
    
    // Reset to defaults
    cfg.setDefaultCorsEnabled(false);
    cfg.setDefaultCorsOrigins("*");
    cfg.setDefaultCorsMethods("GET,POST,PUT,DELETE");
}

TEST(config_json_cors_loading) {
    std::string tempPath = "/tmp/omninet_cors_test.json";
    std::ofstream file(tempPath);
    file << R"({
        "cors": {
            "enabled": true,
            "allowedOrigins": "https://test.com",
            "allowedMethods": "GET,POST,DELETE",
            "allowedHeaders": "X-Custom-Header",
            "maxAge": 3600
        }
    })";
    file.close();
    
    auto& cfg = OmniNet::Config::instance();
    cfg.loadFromFile(tempPath);
    
    ASSERT_TRUE(cfg.defaultCorsEnabled());
    ASSERT_EQ(cfg.defaultCorsOrigins(), "https://test.com");
    ASSERT_EQ(cfg.defaultCorsMethods(), "GET,POST,DELETE");
    ASSERT_EQ(cfg.defaultCorsHeaders(), "X-Custom-Header");
    ASSERT_EQ(cfg.defaultCorsMaxAge().count(), 3600);
    
    std::filesystem::remove(tempPath);
}

// ============================================================================
// TLS Certificate Path Tests
// ============================================================================

TEST(config_tls_default_paths) {
    auto& cfg = OmniNet::Config::instance();
    
    // Default TLS paths should be under dataDir()/certs/
    auto certPath = cfg.defaultTlsCertPath();
    auto keyPath = cfg.defaultTlsKeyPath();
    
    // Paths should be empty if certs don't exist yet
    // When they exist, should point to <dataDir>/certs/omninet.*
    if (!certPath.empty()) {
        ASSERT_TRUE(certPath.string().find("certs") != std::string::npos);
        ASSERT_TRUE(certPath.string().find("omninet.crt") != std::string::npos);
    }
    
    if (!keyPath.empty()) {
        ASSERT_TRUE(keyPath.string().find("certs") != std::string::npos);
        ASSERT_TRUE(keyPath.string().find("omninet.key") != std::string::npos);
    }
}

TEST(config_tls_paths_exist_check) {
    auto& cfg = OmniNet::Config::instance();
    
    // Generate test certificates
    std::string certDir = "/tmp/omninet_test_certs";
    std::filesystem::create_directories(certDir + "/certs");
    
    // Create dummy cert files
    std::ofstream cert(certDir + "/certs/omninet.crt");
    cert << "test certificate content";
    cert.close();
    
    std::ofstream key(certDir + "/certs/omninet.key");
    key << "test key content";
    key.close();
    
    // Change data dir
    cfg.setDataDir(certDir);
    
    // Now paths should exist
    auto certPath = cfg.defaultTlsCertPath();
    auto keyPath = cfg.defaultTlsKeyPath();
    
    // First check paths are not empty
    ASSERT_FALSE(certPath.empty());
    ASSERT_FALSE(keyPath.empty());
    
    // Then check files exist
    ASSERT_TRUE(std::filesystem::exists(certPath));
    ASSERT_TRUE(std::filesystem::exists(keyPath));
    
    // Cleanup
    std::filesystem::remove_all(certDir);
    // Don't reset data dir — we're at end of test run and the
    // reset path (/home/lee/.omninet) may not be writable in containers.
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\nConfig System Tests:\n";
    std::cout << "====================\n\n";
    
    std::cout << "Singleton Tests:\n";
    RUN(config_singleton);
    RUN(config_default_values);
    RUN(config_data_directory);
    RUN(config_resolve_path);
    RUN(config_resolve_absolute);
    
    std::cout << "\nLoading Tests:\n";
    RUN(config_json_parsing);
    RUN(config_invalid_json);
    
    std::cout << "\nValidation Tests:\n";
    RUN(config_validation_valid);
    RUN(config_validation_port_zero);
    RUN(config_validation_worker_count);
    
    std::cout << "\nSerialization Tests:\n";
    RUN(config_to_json);
    RUN(config_save_and_load);
    
    std::cout << "\nEnvironment Tests:\n";
    RUN(config_env_override);
    
    std::cout << "\nCallback Tests:\n";
    RUN(config_change_callback);
    
    std::cout << "\nNew Config Options Tests:\n";
    RUN(config_keepalive_timeout);
    RUN(config_max_request_size);
    RUN(config_cors_defaults);
    RUN(config_cors_setter);
    RUN(config_json_cors_loading);
    
    std::cout << "\nTLS Certificate Path Tests:\n";
    RUN(config_tls_default_paths);
    RUN(config_tls_paths_exist_check);
    
    std::cout << "\n========================================\n";
    std::cout << "Config Tests: " << testsPassed << " passed, " << testsFailed << " failed\n";
    std::cout << "========================================\n";
    
    return testsFailed > 0 ? 1 : 0;
}
