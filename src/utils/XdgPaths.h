// OmniNet - XDG Base Directory path helpers
#ifndef OMNINET_UTILS_XDG_PATHS_H
#define OMNINET_UTILS_XDG_PATHS_H

#include <filesystem>
#include <optional>
#include <string>

namespace OmniNet::Utils {

// ============================================================================
// XDG Base Directory Helpers
//
// Implements the Freedesktop XDG Base Directory Specification as summarized
// in docs/xdg-standard.md. All functions return absolute paths and create
// the requested leaf directory on demand with mode 0700 (existing directories
// are left untouched).
// ============================================================================

// XDG base directories (with spec defaults)
std::filesystem::path configHome();
std::filesystem::path dataHome();
std::filesystem::path stateHome();
std::filesystem::path cacheHome();

// Runtime directory. If XDG_RUNTIME_DIR is unset, falls back to a secure
// user-specific directory under TMPDIR or /tmp and emits a one-time warning
// to stderr. The returned directory is created with mode 0700.
std::filesystem::path runtimeDir();

// OmniNet-specific subdirectories under the XDG base dirs
std::filesystem::path configDir();
std::filesystem::path dataDir();
std::filesystem::path stateDir();
std::filesystem::path cacheDir();
std::filesystem::path runtimeDirSubdir();

// Legacy single-tree layout used by OmniNet before XDG adoption.
// Returns $HOME/.omninet or the OMNINET_DATA_DIR override if set.
std::filesystem::path legacyDataDir();
bool hasLegacyDataDir();

// Returns true if the caller should use the legacy ~/.omninet layout.
// This is true when OMNINET_DATA_DIR is set, or when ~/.omninet exists
// and no explicit XDG override has forced the new layout.
bool useLegacyLayout();

// Effective paths considering legacy override / migration behavior.
// These are the paths the rest of the codebase should consume.
std::filesystem::path effectiveConfigDir();
std::filesystem::path effectiveDataDir();
std::filesystem::path effectiveStateDir();
std::filesystem::path effectiveCacheDir();
std::filesystem::path effectiveRuntimeDirSubdir();
std::filesystem::path effectiveConfigFile();

// Migrate the legacy ~/.omninet tree into the XDG layout.
// On success, returns true and renames ~/.omninet to ~/.omninet.bak.
// On failure, returns false and writes a message to errorOut (if provided).
bool migrateLegacyToXdg(std::string* errorOut = nullptr);

// Internal helpers exposed for testing
namespace detail {
    std::filesystem::path xdgHome(const char* envName,
                                  std::string_view defaultSuffix);
    bool mkdir0700(const std::filesystem::path& path);
    bool mkdirp0700(const std::filesystem::path& path);
} // namespace detail

} // namespace OmniNet::Utils

#endif // OMNINET_UTILS_XDG_PATHS_H
