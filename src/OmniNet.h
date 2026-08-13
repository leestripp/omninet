#pragma once
#ifndef OMNINET_H
#define OMNINET_H
// OmniNet - C++ HTTP/WebSocket Server Library
// Main include file
//
// Project: OmniNet
// Version: Set via CMake (OMNINET_VERSION_*)

// Version macros - defined by CMake
#ifndef OMNINET_VERSION_MAJOR
#define OMNINET_VERSION_MAJOR 0
#endif
#ifndef OMNINET_VERSION_MINOR
#define OMNINET_VERSION_MINOR 1
#endif
#ifndef OMNINET_VERSION_PATCH
#define OMNINET_VERSION_PATCH 0
#endif

#define OMNINET_VERSION_STRING OMNINET_VERSION_MAJOR "." OMNINET_VERSION_MINOR "." OMNINET_VERSION_PATCH

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
#include "net/UdpSocket.h"
#include "net/DtlsSocket.h"
#include "net/DtlsSession.h"

// ============================================================================
// Game Networking Layer (UDP)
// ============================================================================

#include "game/BitStream.h"
#include "game/GameProtocol.h"
#include "game/GameConnection.h"
#include "game/GameServer.h"
#include "game/GameClient.h"
#include "game/DeltaSnapshot.h"

// ============================================================================
// HTTP Layer
// ============================================================================

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/StreamingResponse.h"
#include "http/HttpClient.h"
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

// Global initialization (defined in OmniNet.cpp)
void init();

// Global cleanup (defined in OmniNet.cpp)
void cleanup();

} // namespace OmniNet

#endif // OMNINET_H
