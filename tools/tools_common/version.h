// tools_common/version.h - OmniNet Tools Version
// Shared version information for all CLI tools

#ifndef OMNINET_TOOLS_VERSION_H
#define OMNINET_TOOLS_VERSION_H

#include <string>

// Version components - set by CMake at compile time
// These default to fallback values if not provided by CMake

#ifndef OMNINET_VERSION_MAJOR
#define OMNINET_VERSION_MAJOR 0
#endif

#ifndef OMNINET_VERSION_MINOR
#define OMNINET_VERSION_MINOR 1
#endif

#ifndef OMNINET_VERSION_PATCH
#define OMNINET_VERSION_PATCH 0
#endif

// Build metadata (e.g., "debug", "release", "git-abcd1234")
#ifndef OMNINET_BUILD_TYPE
#define OMNINET_BUILD_TYPE "release"
#endif

// Full version string: "X.Y.Z" or "X.Y.Z-buildtype"
#define OMNINET_TOOLS_VERSION_STRING \
    OMNINET_XSTRING(OMNINET_VERSION_MAJOR) "." \
    OMNINET_XSTRING(OMNINET_VERSION_MINOR) "." \
    OMNINET_XSTRING(OMNINET_VERSION_PATCH)

// Helper macros for stringification
#define OMNINET_XSTRING(x) OMNINET_STRINGIFY(x)
#define OMNINET_STRINGIFY(x) #x

// Convenience macro for version checks
#define OMNINET_TOOLS_CHECK_VERSION(major, minor, patch) \
    (OMNINET_VERSION_MAJOR > (major) || \
     (OMNINET_VERSION_MAJOR == (major) && OMNINET_VERSION_MINOR > (minor)) || \
     (OMNINET_VERSION_MAJOR == (major) && OMNINET_VERSION_MINOR == (minor) && OMNINET_VERSION_PATCH >= (patch)))

// Tool name constants
#define OMNINET_TOOL_NAME "omninet"
#define OMNINET_TOOL_DESCRIPTION "OmniNet Server Management CLI"

// Version helper functions (declared here, defined in version.cpp)
namespace OmniNet {
namespace Tools {
namespace Version {

// Get individual version components
int major();
int minor();
int patch();

// Get full version string
const char* version();

// Get build type
const char* buildType();

// Get full version with build info
std::string fullVersion();

} // namespace Version
} // namespace Tools
} // namespace OmniNet

#endif // OMNINET_TOOLS_VERSION_H
