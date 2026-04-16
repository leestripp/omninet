cmake_minimum_required(VERSION 3.16)
project(OmniNet VERSION 1.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build static library
add_library(OmniNet STATIC)

# Source files for Phase 1 (Foundation)
target_sources(OmniNet PRIVATE
    src/OmniNet.cpp
    src/utils/Exception.cpp
    src/utils/Logger.cpp
    src/utils/Utils.cpp
    src/config/Config.cpp
    src/crypto/Crypto.cpp
    src/crypto/Password.cpp
    src/net/Socket.cpp
    src/net/TLSSocket.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpConnection.cpp
    src/http/Router.cpp
    src/http/HttpServer.cpp
    src/http/StaticHandler.cpp
    src/websocket/WebSocketFrame.cpp
    src/websocket/WebSocketHandshake.cpp
    src/websocket/WebSocket.cpp
    src/db/Database.cpp
    src/db/SessionStore.cpp
    src/db/UserStore.cpp
    src/dashboard/DashboardHandler.cpp
    src/dashboard/RateLimiter.cpp
    src/sse/SSEHandler.cpp
    src/intrusion/IntrusionDetector.cpp
)

# Include directories
target_include_directories(OmniNet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler warnings
target_compile_options(OmniNet PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(OmniNet PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Find SQLite3
find_package(SQLite3 REQUIRED)
target_link_libraries(OmniNet PUBLIC SQLite3::SQLite3)

# Platform-specific sockets (usually part of libc)
target_link_libraries(OmniNet PUBLIC pthread)

# Define version macro
target_compile_definitions(OmniNet PUBLIC
    OMNINET_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    OMNINET_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    OMNINET_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Installation
include(GNUInstallDirs)
install(TARGETS OmniNet
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Testing (optional)
option(OMNINET_BUILD_TESTS "Build tests" ON)
if(OMNINET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Demos (optional)
option(OMNINET_BUILD_DEMOS "Build demos" ON)
if(OMNINET_BUILD_DEMOS)
    add_subdirectory(demos)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "OmniNet Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  SQLite3: ${SQLite3_VERSION}")
message(STATUS "")
