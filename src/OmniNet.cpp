// OmniNet - Version Info and Global Initialization
#include "OmniNet.h"

namespace OmniNet {

// Global initialization (call once at program start)
void init() {
    Crypto::initialize();
    
    // Initialize config - this creates the XDG OmniNet directories and config.json
    Config::instance();
}

// Global cleanup (call once at program end)
void cleanup() {
    Crypto::cleanup();
    
    // Stop config file watcher
    Config::instance().stopFileWatcher();
}

} // namespace OmniNet