// OmniNet XDG Base Directory path helpers tests
#include "utils/XdgPaths.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using namespace OmniNet::Utils;

int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN(name) do { std::cout << "  " #name "... "; test_##name(); std::cout << "PASS\n"; testsPassed++; } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { std::cout << "FAIL (" #cond ")"; testsFailed++; } } while(0)
#define ASSERT_FALSE(cond) do { if (cond) { std::cout << "FAIL (" #cond ")"; testsFailed++; } } while(0)
#define ASSERT_EQ(a, b) do { auto _a = (a); auto _b = (b); if (_a != _b) { std::cout << "FAIL: " #a " (" << _a << ") != " << _b; testsFailed++; } } while(0)
#define ASSERT_NE(a, b) do { auto _a = (a); auto _b = (b); if (_a == _b) { std::cout << "FAIL: " #a " == " << _b; testsFailed++; } } while(0)

// ============================================================================
// Environment isolation helpers
// ============================================================================

class EnvGuard {
public:
    EnvGuard() = default;
    ~EnvGuard() { restoreAll(); }

    void set(const char* name, const char* value) {
        auto it = m_original.find(name);
        if (it == m_original.end()) {
            const char* old = std::getenv(name);
            m_original[name] = old != nullptr ? std::optional<std::string>(old) : std::nullopt;
        }
        if (value != nullptr) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    void unset(const char* name) {
        set(name, nullptr);
    }

    void restoreAll() {
        for (const auto& [name, value] : m_original) {
            if (value) {
                setenv(name.c_str(), value->c_str(), 1);
            } else {
                unsetenv(name.c_str());
            }
        }
        m_original.clear();
    }

private:
    std::map<std::string, std::optional<std::string>> m_original;
};

fs::path makeTempHome(const std::string& prefix) {
    fs::path tmp = fs::temp_directory_path() / prefix;
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    return tmp;
}

// ============================================================================
// Tests
// ============================================================================

TEST(xdg_defaults) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_defaults");
    env.set("HOME", home.string().c_str());
    env.unset("XDG_CONFIG_HOME");
    env.unset("XDG_DATA_HOME");
    env.unset("XDG_STATE_HOME");
    env.unset("XDG_CACHE_HOME");
    env.unset("XDG_RUNTIME_DIR");

    ASSERT_EQ(configHome(), home / ".config");
    ASSERT_EQ(dataHome(), home / ".local/share");
    ASSERT_EQ(stateHome(), home / ".local/state");
    ASSERT_EQ(cacheHome(), home / ".cache");
}

TEST(xdg_env_overrides) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_env");
    env.set("HOME", home.string().c_str());
    env.set("XDG_CONFIG_HOME", "/cfg");
    env.set("XDG_DATA_HOME", "/data");
    env.set("XDG_STATE_HOME", "/state");
    env.set("XDG_CACHE_HOME", "/cache");
    env.set("XDG_RUNTIME_DIR", "/runtime");

    ASSERT_EQ(configHome(), fs::path("/cfg"));
    ASSERT_EQ(dataHome(), fs::path("/data"));
    ASSERT_EQ(stateHome(), fs::path("/state"));
    ASSERT_EQ(cacheHome(), fs::path("/cache"));
    ASSERT_EQ(runtimeDir(), fs::path("/runtime"));
}

TEST(xdg_empty_env_falls_back) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_empty");
    env.set("HOME", home.string().c_str());
    env.set("XDG_DATA_HOME", "");
    env.set("XDG_CONFIG_HOME", "");
    env.set("XDG_STATE_HOME", "");
    env.set("XDG_CACHE_HOME", "");

    ASSERT_EQ(dataHome(), home / ".local/share");
    ASSERT_EQ(configHome(), home / ".config");
    ASSERT_EQ(stateHome(), home / ".local/state");
    ASSERT_EQ(cacheHome(), home / ".cache");
}

TEST(xdg_relative_env_rejected) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_relative");
    env.set("HOME", home.string().c_str());
    env.set("XDG_DATA_HOME", "relative/path");
    env.set("XDG_CONFIG_HOME", "relative/path");

    ASSERT_EQ(dataHome(), home / ".local/share");
    ASSERT_EQ(configHome(), home / ".config");
}

TEST(xdg_directories_created) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_dirs");
    env.set("HOME", home.string().c_str());
    env.unset("XDG_DATA_HOME");
    env.unset("XDG_CONFIG_HOME");
    env.unset("XDG_STATE_HOME");
    env.unset("XDG_CACHE_HOME");
    env.set("XDG_RUNTIME_DIR", (home / "runtime").string().c_str());

    auto dat = dataDir();
    auto cfg = configDir();
    auto st = stateDir();
    auto ca = cacheDir();
    auto rt = runtimeDirSubdir();

    ASSERT_TRUE(fs::exists(dat));
    ASSERT_TRUE(fs::exists(cfg));
    ASSERT_TRUE(fs::exists(st));
    ASSERT_TRUE(fs::exists(ca));
    ASSERT_TRUE(fs::exists(rt));

    ASSERT_EQ(dat, home / ".local/share/omninet");
    ASSERT_EQ(cfg, home / ".config/omninet");
    ASSERT_EQ(st, home / ".local/state/omninet");
    ASSERT_EQ(ca, home / ".cache/omninet");
    ASSERT_EQ(rt, home / "runtime/omninet");
}

TEST(xdg_legacy_detection) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_legacy");
    env.set("HOME", home.string().c_str());
    env.unset("OMNINET_DATA_DIR");

    // No legacy dir: should not use legacy layout.
    ASSERT_FALSE(hasLegacyDataDir());
    ASSERT_FALSE(useLegacyLayout());

    // Create legacy dir.
    fs::create_directories(home / ".omninet");
    ASSERT_TRUE(hasLegacyDataDir());
    ASSERT_TRUE(useLegacyLayout());

    // OMNINET_DATA_DIR override forces legacy.
    env.set("OMNINET_DATA_DIR", "/some/other/path");
    ASSERT_TRUE(useLegacyLayout());
    ASSERT_EQ(legacyDataDir(), fs::path("/some/other/path"));
}

TEST(xdg_migration) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_migrate");
    env.set("HOME", home.string().c_str());
    env.unset("OMNINET_DATA_DIR");
    env.unset("XDG_DATA_HOME");
    env.unset("XDG_CONFIG_HOME");
    env.unset("XDG_STATE_HOME");
    env.unset("XDG_CACHE_HOME");
    env.set("XDG_RUNTIME_DIR", (home / "runtime").string().c_str());

    // Populate legacy tree.
    fs::create_directories(home / ".omninet/certs");
    fs::create_directories(home / ".omninet/servers");
    {
        std::ofstream(home / ".omninet/config.json") << "{\"version\": \"1.0\"}";
        std::ofstream(home / ".omninet/omninet.db") << "db";
        std::ofstream(home / ".omninet/omninet.log") << "log";
        std::ofstream(home / ".omninet/omninet.log.1") << "rotated";
        std::ofstream(home / ".omninet/omninet-server.pid") << "1234";
    }

    std::string error;
    ASSERT_TRUE(migrateLegacyToXdg(&error));

    ASSERT_TRUE(fs::exists(home / ".config/omninet/config.json"));
    ASSERT_TRUE(fs::exists(home / ".local/share/omninet/omninet.db"));
    ASSERT_TRUE(fs::exists(home / ".local/share/omninet/certs"));
    ASSERT_TRUE(fs::exists(home / ".local/share/omninet/servers"));
    ASSERT_TRUE(fs::exists(home / ".local/state/omninet/omninet.log"));
    ASSERT_TRUE(fs::exists(home / ".local/state/omninet/omninet.log.1"));
    ASSERT_TRUE(fs::exists(home / "runtime/omninet/omninet-server.pid"));
    ASSERT_TRUE(fs::exists(home / ".omninet.bak"));
    ASSERT_FALSE(fs::exists(home / ".omninet"));
}

TEST(xdg_effective_paths) {
    EnvGuard env;
    auto home = makeTempHome("omninet_xdg_effective");
    env.set("HOME", home.string().c_str());
    env.unset("OMNINET_DATA_DIR");
    env.unset("XDG_DATA_HOME");
    env.unset("XDG_CONFIG_HOME");
    env.unset("XDG_STATE_HOME");
    env.unset("XDG_CACHE_HOME");
    env.set("XDG_RUNTIME_DIR", (home / "runtime").string().c_str());

    // No legacy dir: effective paths are XDG paths.
    ASSERT_EQ(effectiveConfigFile(), home / ".config/omninet/config.json");
    ASSERT_EQ(effectiveDataDir(), home / ".local/share/omninet");
    ASSERT_EQ(effectiveStateDir(), home / ".local/state/omninet");
    ASSERT_EQ(effectiveCacheDir(), home / ".cache/omninet");
    ASSERT_EQ(effectiveRuntimeDirSubdir(), home / "runtime/omninet");

    // Create legacy dir: effective paths switch to legacy tree.
    fs::create_directories(home / ".omninet");
    ASSERT_EQ(effectiveConfigFile(), home / ".omninet/config.json");
    ASSERT_EQ(effectiveDataDir(), home / ".omninet");
    ASSERT_EQ(effectiveStateDir(), home / ".omninet");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\nXDG Paths Tests:\n";
    std::cout << "================\n\n";

    RUN(xdg_defaults);
    RUN(xdg_env_overrides);
    RUN(xdg_empty_env_falls_back);
    RUN(xdg_relative_env_rejected);
    RUN(xdg_directories_created);
    RUN(xdg_legacy_detection);
    RUN(xdg_migration);
    RUN(xdg_effective_paths);

    std::cout << "\n========================================\n";
    std::cout << "XDG Paths Tests: " << testsPassed << " passed, " << testsFailed << " failed\n";
    std::cout << "========================================\n";

    return testsFailed > 0 ? 1 : 0;
}
