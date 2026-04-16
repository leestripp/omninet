// OmniNet - Version Info
#include "OmniNet.h"

namespace OmniNet {

// Global initialization flag
static bool s_initialized = false;

void init() {
    if (!s_initialized) {
        Crypto::initialize();
        s_initialized = true;
    }
}

void cleanup() {
    if (s_initialized) {
        Crypto::cleanup();
        s_initialized = false;
    }
}

} // namespace OmniNet
