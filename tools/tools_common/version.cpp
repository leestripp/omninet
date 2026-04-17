// tools_common/version.cpp - OmniNet Tools Version Implementation

#include "version.h"

namespace OmniNet {
namespace Tools {
namespace Version {

int major() {
    return OMNINET_VERSION_MAJOR;
}

int minor() {
    return OMNINET_VERSION_MINOR;
}

int patch() {
    return OMNINET_VERSION_PATCH;
}

const char* version() {
    return OMNINET_TOOLS_VERSION_STRING;
}

const char* buildType() {
    return OMNINET_BUILD_TYPE;
}

std::string fullVersion() {
    std::string ver = OMNINET_TOOLS_VERSION_STRING;
    #ifdef OMNINET_GIT_COMMIT
    ver += "-git:" + std::string(OMNINET_GIT_COMMIT);
    #endif
    ver += " [";
    ver += OMNINET_BUILD_TYPE;
    ver += "]";
    return ver;
}

} // namespace Version
} // namespace Tools
} // namespace OmniNet
