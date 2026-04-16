// OmniNet - C++ HTTP/WebSocket Server Library
// Main include file
//
// Project: OmniNet
// Version: 1.0.0

#ifndef OMNINET_H
#define OMNINET_H

// Version macros
#define OMNINET_VERSION_MAJOR 1
#define OMNINET_VERSION_MINOR 0
#define OMNINET_VERSION_PATCH 0

#define OMNINET_VERSION_STRING "1.0.0"

// Convenience macro for version checks
#define OMNINET_CHECK_VERSION(major, minor, patch) \
    (OMNINET_VERSION_MAJOR > (major) || \
     (OMNINET_VERSION_MAJOR == (major) && OMNINET_VERSION_MINOR > (minor)) || \
     (OMNINET_VERSION_MAJOR == (major) && OMNINET_VERSION_MINOR == (minor) && OMNINET_VERSION_PATCH >= (patch)))

// ============================================================================
// Foundation Layer
// ============================================================================

// Exception hierarchy
#include "utils/Exception.h"

// Logging system
#include "utils/Logger.h"

// Utilities
#include "utils/Utils.h"

// Configuration
#include "config/Config.h"

// ============================================================================
// Cryptography Layer
// ============================================================================

#include "crypto/Crypto.h"
#include "crypto/Password.h"

// ============================================================================
// Networking Layer
// ============================================================================

#include "net/Socket.h"
#include "net/TLSSocket.h"

// ============================================================================
// HTTP Layer
// ============================================================================

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/HttpConnection.h"
#include "http/Router.h"
#include "http/HttpServer.h"
#include "http/StaticHandler.h"

// ============================================================================
// WebSocket Layer
// ============================================================================

#include "websocket/WebSocketFrame.h"
#include "websocket/WebSocketHandshake.h"
#include "websocket/WebSocket.h"
#include "websocket/WebSocketServer.h"

// ============================================================================
// Database Layer
// ============================================================================

#include "db/Database.h"
#include "db/SessionStore.h"
#include "db/UserStore.h"

// ============================================================================
// Dashboard Layer
// ============================================================================

#include "dashboard/DashboardHandler.h"
#include "dashboard/RateLimiter.h"

// ============================================================================
// Server-Sent Events
// ============================================================================

#include "sse/SSEHandler.h"

// ============================================================================
// Intrusion Detection
// ============================================================================

#include "intrusion/IntrusionDetector.h"

namespace OmniNet {

// Global initialization (call once at program start)
inline void init() {
    Crypto::initialize();
}

// Global cleanup (call once at program end)
inline void cleanup() {
    Crypto::cleanup();
}

} // namespace OmniNet

#endif // OMNINET_H
