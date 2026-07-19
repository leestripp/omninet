// OmniNet - XDG Base Directory path helpers implementation
#include "XdgPaths.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace OmniNet::Utils {

namespace {

// ============================================================================
// Platform / environment helpers
// ============================================================================

const char* homeDir() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') return home;

#ifdef _WIN32
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile != nullptr && userProfile[0] != '\0') return userProfile;
#endif

    return "/tmp";
}

bool isAbsolutePath(std::string_view path) {
    return !path.empty() && path.front() == '/';
}

#ifdef _WIN32
bool isAbsolutePathWin(std::string_view path) {
    if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    return isAbsolutePath(path);
}
#endif

// ============================================================================
// Directory creation helpers
// ============================================================================

bool makeDir(const std::string& path) {
#ifdef _WIN32
    return ::_mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return ::mkdir(path.c_str(), 0700) == 0 || errno == EEXIST;
#endif
}

} // anonymous namespace

namespace detail {

fs::path xdgHome(const char* envName, std::string_view defaultSuffix) {
    const char* envValue = std::getenv(envName);
    if (envValue != nullptr && envValue[0] != '\0') {
        std::string_view value(envValue);
#ifdef _WIN32
        if (isAbsolutePathWin(value)) {
            return fs::path{value};
        }
#else
        if (isAbsolutePath(value)) {
            return fs::path{value};
        }
#endif
        // Relative or invalid value: fall through to default.
    }
    return fs::path{homeDir()} / defaultSuffix;
}

bool mkdir0700(const fs::path& path) {
    return makeDir(path.string());
}

bool mkdirp0700(const fs::path& path) {
    if (path.empty()) return false;

    std::error_code ec;
    if (fs::exists(path, ec) && fs::is_directory(path, ec)) {
        return true;
    }

    // Walk components and create each missing one with 0700.
    fs::path current;
    for (const auto& part : path) {
        current /= part;
        if (current == "/") continue;

        if (!fs::exists(current, ec)) {
            if (!makeDir(current.string())) {
                return false;
            }
        }
    }
    return fs::is_directory(path, ec);
}

} // namespace detail

// ============================================================================
// Base XDG directories
// ============================================================================

fs::path configHome() {
    return detail::xdgHome("XDG_CONFIG_HOME", ".config");
}

fs::path dataHome() {
    return detail::xdgHome("XDG_DATA_HOME", ".local/share");
}

fs::path stateHome() {
    return detail::xdgHome("XDG_STATE_HOME", ".local/state");
}

fs::path cacheHome() {
    return detail::xdgHome("XDG_CACHE_HOME", ".cache");
}

fs::path runtimeDir() {
    const char* envValue = std::getenv("XDG_RUNTIME_DIR");
    if (envValue != nullptr && envValue[0] != '\0') {
#ifdef _WIN32
        if (isAbsolutePathWin(envValue)) {
            return fs::path{envValue};
        }
#else
        if (isAbsolutePath(envValue)) {
            return fs::path{envValue};
        }
#endif
    }

    // Fallback to TMPDIR or /tmp with a user-specific subdirectory.
    static std::once_flag warnOnce;
    std::call_once(warnOnce, []() {
        std::cerr << "warning: XDG_RUNTIME_DIR is not set or invalid; "
                  << "using a fallback runtime directory.\n";
    });

    const char* tmp = std::getenv("TMPDIR");
    if (tmp == nullptr || tmp[0] == '\0') tmp = "/tmp";

    fs::path fallback;
#ifdef _WIN32
    fallback = fs::path{tmp} / ("omninet-runtime-" + std::to_string(GetCurrentProcessId()));
#else
    fallback = fs::path{tmp} / ("omninet-runtime-" + std::to_string(getuid()));
#endif
    detail::mkdirp0700(fallback);
    return fallback;
}

// ============================================================================
// OmniNet-specific subdirectories
// ============================================================================

fs::path configDir() {
    fs::path dir = configHome() / "omninet";
    detail::mkdirp0700(dir);
    return dir;
}

fs::path dataDir() {
    fs::path dir = dataHome() / "omninet";
    detail::mkdirp0700(dir);
    return dir;
}

fs::path stateDir() {
    fs::path dir = stateHome() / "omninet";
    detail::mkdirp0700(dir);
    return dir;
}

fs::path cacheDir() {
    fs::path dir = cacheHome() / "omninet";
    detail::mkdirp0700(dir);
    return dir;
}

fs::path runtimeDirSubdir() {
    fs::path dir = runtimeDir() / "omninet";
    detail::mkdirp0700(dir);
    return dir;
}

// ============================================================================
// Legacy layout support
// ============================================================================

fs::path legacyDataDir() {
    const char* override = std::getenv("OMNINET_DATA_DIR");
    if (override != nullptr && override[0] != '\0') {
        return fs::path{override};
    }
    return fs::path{homeDir()} / ".omninet";
}

bool hasLegacyDataDir() {
    std::error_code ec;
    return fs::exists(legacyDataDir(), ec) && fs::is_directory(legacyDataDir(), ec);
}

bool useLegacyLayout() {
    const char* override = std::getenv("OMNINET_DATA_DIR");
    if (override != nullptr && override[0] != '\0') {
        return true;
    }
    return hasLegacyDataDir();
}

// ============================================================================
// Effective paths
// ============================================================================

fs::path effectiveConfigDir() {
    if (useLegacyLayout()) {
        return legacyDataDir();
    }
    return configDir();
}

fs::path effectiveDataDir() {
    if (useLegacyLayout()) {
        return legacyDataDir();
    }
    return dataDir();
}

fs::path effectiveStateDir() {
    if (useLegacyLayout()) {
        return legacyDataDir();
    }
    return stateDir();
}

fs::path effectiveCacheDir() {
    if (useLegacyLayout()) {
        return legacyDataDir();
    }
    return cacheDir();
}

fs::path effectiveRuntimeDirSubdir() {
    if (useLegacyLayout()) {
        return legacyDataDir();
    }
    return runtimeDirSubdir();
}

fs::path effectiveConfigFile() {
    return effectiveConfigDir() / "config.json";
}

// ============================================================================
// Migration from legacy ~/.omninet to XDG layout
// ============================================================================

bool migrateLegacyToXdg(std::string* errorOut) {
    fs::path legacy = legacyDataDir();
    std::error_code ec;

    if (!fs::exists(legacy, ec) || !fs::is_directory(legacy, ec)) {
        if (errorOut != nullptr) *errorOut = "Legacy directory does not exist: " + legacy.string();
        return false;
    }

    auto cfgDir = configDir();
    auto datDir = dataDir();
    auto stDir = stateDir();
    auto caDir = cacheDir();
    auto rtDir = runtimeDirSubdir();

    auto moveFile = [&](const fs::path& from, const fs::path& to, bool required = false) -> bool {
        std::error_code existsEc;
        if (!fs::exists(from, existsEc)) return true;
        try {
            std::error_code renameEc;
            fs::rename(from, to, renameEc);
            if (renameEc) {
                // Cross-device link? Fall back to copy + remove.
                if (renameEc == std::errc::cross_device_link ||
                    renameEc.value() == EXDEV) {
                    fs::copy(from, to, fs::copy_options::overwrite_existing, renameEc);
                    if (!renameEc) {
                        fs::remove(from, renameEc);
                    }
                }
            }
            if (renameEc && required) {
                if (errorOut != nullptr) {
                    *errorOut = "Failed to move " + from.string() + " to " + to.string() + ": " + renameEc.message();
                }
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            if (required) {
                if (errorOut != nullptr) *errorOut = std::string("Exception moving ") + from.string() + ": " + e.what();
                return false;
            }
            return true;
        }
    };

    // Config
    if (!moveFile(legacy / "config.json", cfgDir / "config.json", true)) return false;

    // Data
    if (!moveFile(legacy / "omninet.db", datDir / "omninet.db")) return false;
    if (!moveFile(legacy / "certs", datDir / "certs")) return false;
    if (!moveFile(legacy / "servers", datDir / "servers")) return false;

    // State
    {
        std::error_code iterEc;
        for (const auto& entry : fs::directory_iterator(legacy, iterEc)) {
            const auto& name = entry.path().filename().string();
            if (name.starts_with("omninet.log")) {
                if (!moveFile(entry.path(), stDir / name)) return false;
            }
        }
    }

    // Runtime (PID files)
    {
        std::error_code iterEc;
        for (const auto& entry : fs::directory_iterator(legacy, iterEc)) {
            const auto& name = entry.path().filename().string();
            if (name.ends_with(".pid")) {
                if (!moveFile(entry.path(), rtDir / name)) return false;
            }
        }
    }

    // Cache (nothing to migrate by default, but ensure dir exists)
    detail::mkdirp0700(caDir);

    // Rename legacy tree as backup, or remove if empty.
    fs::path backup = legacy;
    backup += ".bak";
    fs::rename(legacy, backup, ec);
    if (ec) {
        if (errorOut != nullptr) {
            *errorOut = "Migration succeeded but failed to rename legacy dir " + legacy.string() +
                        " to " + backup.string() + ": " + ec.message();
        }
        return false;
    }

    return true;
}

} // namespace OmniNet::Utils
