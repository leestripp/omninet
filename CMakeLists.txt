cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Version Detection from Git Tags
# ============================================================================

# Get version from git tag if available, otherwise use fallback
set(FALLBACK_VERSION "0.1.0")

# Try to get version from git tag
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND git describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Remove 'v' prefix if present
    string(REGEX REPLACE "^v" "" GIT_TAG "${GIT_TAG}")
    
    if(GIT_TAG)
        # Parse version components
        string(REGEX MATCH "^([0-9]+)\.([0-9]+)\.([0-9]+)" VERSION_MATCH "${GIT_TAG}")
        if(VERSION_MATCH)
            string(REGEX REPLACE "^([0-9]+)\.([0-9]+)\.([0-9]+).*" "\\1" DETECTED_MAJOR "${GIT_TAG}")
            string(REGEX REPLACE "^([0-9]+)\.([0-9]+)\.([0-9]+).*" "\\2" DETECTED_MINOR "${GIT_TAG}")
            string(REGEX REPLACE "^([0-9]+)\.([0-9]+)\.([0-9]+).*" "\\3" DETECTED_PATCH "${GIT_TAG}")
        endif()
    endif()
endif()

# Use detected version or fallback
# Note: CMake treats "0" as FALSE in boolean context, so we check NOT DEFINED
if(DEFINED DETECTED_MAJOR AND DEFINED DETECTED_MINOR AND DEFINED DETECTED_PATCH)
    string(LENGTH "${DETECTED_MAJOR}" MAJOR_LEN)
    string(LENGTH "${DETECTED_MINOR}" MINOR_LEN)
    string(LENGTH "${DETECTED_PATCH}" PATCH_LEN)
    if(MAJOR_LEN GREATER 0 AND MINOR_LEN GREATER 0 AND PATCH_LEN GREATER 0)
        set(PROJECT_VERSION_MAJOR ${DETECTED_MAJOR})
        set(PROJECT_VERSION_MINOR ${DETECTED_MINOR})
        set(PROJECT_VERSION_PATCH ${DETECTED_PATCH})
        set(PROJECT_VERSION "${DETECTED_MAJOR}.${DETECTED_MINOR}.${DETECTED_PATCH}")
    else()
        set(PROJECT_VERSION "${FALLBACK_VERSION}")
    endif()
else()
    set(PROJECT_VERSION "${FALLBACK_VERSION}")
endif()

message(STATUS "OmniNet Version: ${PROJECT_VERSION} (from git tag: ${GIT_TAG})")

# ============================================================================
# Project Setup
# ============================================================================

project(OmniNet VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build static library
add_library(OmniNet STATIC)

# Source files - Working modules
target_sources(OmniNet PRIVATE
    # Foundation
    src/OmniNet.cpp
    src/config/Config.cpp
    src/utils/Logger.cpp
    src/utils/LogReader.cpp
    src/utils/Exception.cpp
    src/utils/Utils.cpp
    
    # Crypto
    src/crypto/Crypto.cpp
    src/crypto/Password.cpp
    
    # Networking
    src/net/Socket.cpp
    src/net/TLSSocket.cpp
    src/net/Buffer.cpp
    
    # HTTP
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpConnection.cpp
    src/http/Router.cpp
    src/http/HttpServer.cpp
    src/http/StaticHandler.cpp
    
    # WebSocket
    src/websocket/WebSocketFrame.cpp
    src/websocket/WebSocketHandshake.cpp
    src/websocket/WebSocket.cpp
    
    # Dashboard
    src/dashboard/RateLimiter.cpp
    src/dashboard/DashboardHandler.cpp
    
    # Admin
    src/admin/AdminHandler.cpp
    
    # Database
    src/db/Database.cpp
    src/db/SessionStore.cpp
    src/db/UserStore.cpp
    
    # SSE
    src/sse/SSEHandler.cpp
    
    # Intrusion
    src/intrusion/IntrusionDetector.cpp
)

# Include directories
target_include_directories(OmniNet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(OmniNet PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Find SQLite3
find_package(SQLite3 REQUIRED)
target_link_libraries(OmniNet PUBLIC SQLite3::SQLite3)

# Platform-specific sockets (usually part of libc)
target_link_libraries(OmniNet PUBLIC pthread)

# Fetch nlohmann/json for config parsing (header-only)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(OmniNet PUBLIC nlohmann_json::nlohmann_json)

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

message(STATUS "")
message(STATUS "OmniNet Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  SQLite3: ${SQLite3_VERSION}")
message(STATUS "")

# Enable testing
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Build demos
add_subdirectory(demos)

# Build CLI tools
add_subdirectory(tools)
