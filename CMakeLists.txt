cmake_minimum_required(VERSION 3.16)

# Add local cmake modules to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# Compiler Selection
# ============================================================================
# Prefer gcc/g++ when no compiler was forced by the user. The user can still
# override with CC/CXX or -DCMAKE_C_COMPILER/-DCMAKE_CXX_COMPILER.
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(_OMNINET_GCC gcc)
    find_program(_OMNINET_GXX g++)
    if(_OMNINET_GCC AND _OMNINET_GXX)
        message(STATUS "OmniNet: defaulting to gcc/g++ (set CC/CXX to override)")
        set(CMAKE_C_COMPILER   "${_OMNINET_GCC}" CACHE STRING "C compiler")
        set(CMAKE_CXX_COMPILER "${_OMNINET_GXX}" CACHE STRING "C++ compiler")
    endif()
    unset(_OMNINET_GCC)
    unset(_OMNINET_GXX)
endif()

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
        # Parse fallback "X.Y.Z" into DETECTED_* for compile defs
        string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1" DETECTED_MAJOR "${FALLBACK_VERSION}")
        string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\2" DETECTED_MINOR "${FALLBACK_VERSION}")
        string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\3" DETECTED_PATCH "${FALLBACK_VERSION}")
    endif()
else()
    set(PROJECT_VERSION "${FALLBACK_VERSION}")
    # Parse fallback "X.Y.Z" into DETECTED_* for compile defs
    string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\.([0-9]+).*" "\\1" DETECTED_MAJOR "${FALLBACK_VERSION}")
    string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\.([0-9]+).*" "\\2" DETECTED_MINOR "${FALLBACK_VERSION}")
endif()

message(STATUS "OmniNet Version: ${PROJECT_VERSION} (from git tag: ${GIT_TAG})")

# ============================================================================
# Project Setup
# ============================================================================

project(OmniNet VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

# C++23 required (Phase 1 of async refactor)
# Compiler requirements: GCC 13+ (default), Clang 17+, or MSVC 19.36+.
# Default is set above to gcc/g++ when available; users can override with
# CC=clang CXX=clang++ cmake -S . -B build.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Verify C++23 support and required features
include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag("-std=c++23" CXX23_SUPPORTED)
    if(NOT CXX23_SUPPORTED)
        message(FATAL_ERROR
            "OmniNet requires C++23 support. "
            "Please use GCC 13+ or Clang 17+ (current: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
    endif()

    # Ensure coroutines support
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag("-fcoroutines" HAS_COROUTINES)
        if(NOT HAS_COROUTINES)
            message(FATAL_ERROR
                "Compiler does not support -fcoroutines. "
                "GCC requires version 10+ for C++20 coroutines.")
        endif()
    endif()
endif()

# Enable standard warning flags for all targets in the project.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

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
    src/utils/XdgPaths.cpp
    
    # Crypto
    src/crypto/Crypto.cpp
    src/crypto/Password.cpp
    
    # Networking
    src/net/Socket.cpp
    src/net/TLSSocket.cpp
    src/net/UdpSocket.cpp
    src/net/DtlsSocket.cpp
    src/net/Buffer.cpp

    # Game Networking (UDP)
    src/game/BitStream.cpp
    src/game/GameProtocol.cpp
    src/game/GameConnection.cpp
    src/game/GameServer.cpp
    src/game/GameClient.cpp
    src/game/DeltaSnapshot.cpp
    
    # HTTP
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/StreamingResponse.cpp
    src/http/HttpConnection.cpp
    src/http/HttpClient.cpp
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
    src/db/AsyncDatabase.cpp
    src/db/DatabasePool.cpp
    
    # Vector (sqlite-vec)
    src/vector/VectorStore.cpp
    
    # Thread Pool (Phase 2)
    src/thread/ThreadPool.cpp
    
    # Async (Phase 3) - mostly header only
    src/async/AsyncLogger.cpp
    src/async/AsyncTlsSocket.cpp
    
    # I/O (Phase 4)
    src/io/EpollContext.cpp
    src/io/AsyncSocket.cpp
    src/io/AsyncFile.cpp
    
    # Async Server (Phase 5)
    src/server/AsyncServer.cpp
    
    # Token Management
    src/token/TokenUtils.cpp
    src/token/SessionToken.cpp
    src/token/SessionTokenStore.cpp
    src/token/ApiKey.cpp
    src/token/ApiKeyStore.cpp
    
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
target_link_libraries(OmniNet PUBLIC SQLite::SQLite3)

# Optional zlib for gzip compression
# v0.6.0 fix (bug #3): declare OMNINET_HAVE_ZLIB as a cache option so
# consumers can override it via -DOMNINET_HAVE_ZLIB=OFF. Previously the
# variable was just set inline, making the cache value useless and forcing
# consumers who wanted to disable gzip to post-process the target.
option(OMNINET_HAVE_ZLIB "Enable gzip response compression via zlib" ON)
# Use find_library/find_path for compatibility with standard zlib1g-dev package
find_path(ZLIB_INCLUDE_DIR NAMES zlib.h)
find_library(ZLIB_LIBRARY NAMES z)
if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
    if(OMNINET_HAVE_ZLIB)
        set(Zlib_FOUND TRUE)
        add_library(Zlib::Zlib SHARED IMPORTED)
        set_target_properties(Zlib::Zlib PROPERTIES
            IMPORTED_LOCATION "${ZLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
        )
        target_link_libraries(OmniNet PUBLIC Zlib::Zlib)
        target_compile_definitions(OmniNet PUBLIC OMNINET_HAVE_ZLIB=1)
        message(STATUS "zlib found - gzip compression enabled")
    else()
        target_compile_definitions(OmniNet PUBLIC OMNINET_HAVE_ZLIB=0)
        message(STATUS "zlib found - gzip compression disabled (per OMNINET_HAVE_ZLIB=OFF)")
    endif()
else()
    target_compile_definitions(OmniNet PUBLIC OMNINET_HAVE_ZLIB=0)
    message(STATUS "zlib not found - gzip compression disabled")
endif()

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

# ============================================================================
# sqlite-vec Vector Database Extension (FetchContent)
# ============================================================================
# sqlite-vec provides vector storage and similarity search for semantic search.
# It is fetched, built, and loaded at runtime via SQLite's load_extension().
option(OMNINET_ENABLE_VEC "Enable sqlite-vec vector database support" ON)

if(OMNINET_ENABLE_VEC)
    FetchContent_Declare(
        sqlite_vec
        GIT_REPOSITORY https://github.com/asg017/sqlite-vec.git
        GIT_TAG v0.1.6
        GIT_SHALLOW TRUE
    )
    
    FetchContent_MakeAvailable(sqlite_vec)
    
    # Get the actual source directory
    FetchContent_GetProperties(sqlite_vec)
    
    # Generate sqlite-vec.h from sqlite-vec.h.tmpl
    set(VEC_H_TMPL "${sqlite_vec_SOURCE_DIR}/sqlite-vec.h.tmpl")
    set(VEC_H "${sqlite_vec_SOURCE_DIR}/sqlite-vec.h")
    set(VEC_VERSION_FILE "${sqlite_vec_SOURCE_DIR}/VERSION")
    
    # Read version
    file(READ "${VEC_VERSION_FILE}" VEC_VERSION)
    string(STRIP "${VEC_VERSION}" VEC_VERSION)
    string(REPLACE "." ";" VERSION_PARTS "${VEC_VERSION}")
    list(GET VERSION_PARTS 0 VERSION_MAJOR)
    list(GET VERSION_PARTS 1 VERSION_MINOR)
    list(LENGTH VERSION_PARTS VERSION_PARTS_LEN)
    if(VERSION_PARTS_LEN EQUAL 3)
        list(GET VERSION_PARTS 2 VERSION_PATCH)
    else()
        set(VERSION_PATCH "0")
    endif()
    
    # Read template and substitute ${VAR} style variables
    file(READ "${VEC_H_TMPL}" VEC_H_CONTENT)
    string(REPLACE "\${VERSION}" "${VEC_VERSION}" VEC_H_CONTENT "${VEC_H_CONTENT}")
    string(REPLACE "\${VERSION_MAJOR}" "${VERSION_MAJOR}" VEC_H_CONTENT "${VEC_H_CONTENT}")
    string(REPLACE "\${VERSION_MINOR}" "${VERSION_MINOR}" VEC_H_CONTENT "${VEC_H_CONTENT}")
    string(REPLACE "\${VERSION_PATCH}" "${VERSION_PATCH}" VEC_H_CONTENT "${VEC_H_CONTENT}")
    string(REPLACE "\${DATE}" "unknown" VEC_H_CONTENT "${VEC_H_CONTENT}")
    string(REPLACE "\${SOURCE}" "unknown" VEC_H_CONTENT "${VEC_H_CONTENT}")
    file(WRITE "${VEC_H}" "${VEC_H_CONTENT}")
    
    # Build vec0.so as a CMake shared library target
    # Note: OUTPUT_NAME "vec0" with SHARED library type automatically gets "lib" prefix on Linux
    # We use RENAME to override this behavior
    add_library(vec0_extension SHARED ${sqlite_vec_SOURCE_DIR}/sqlite-vec.c)
    target_include_directories(vec0_extension PUBLIC ${sqlite_vec_SOURCE_DIR})
    # sqlite-vec is third-party code; suppress its noisy C-only warnings.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(vec0_extension PRIVATE
            -Wno-c23-extensions
            -Wno-unterminated-string-initialization
        )
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(vec0_extension PRIVATE
            -Wno-unterminated-string-initialization
            -Wno-sign-compare
            -Wno-type-limits
        )
    endif()
    set_target_properties(vec0_extension PROPERTIES
        OUTPUT_NAME "vec0"  # Results in libvec0.so on Linux
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # Store extension path for runtime use
    set(OMNINET_VEC0_PATH "${CMAKE_BINARY_DIR}/libvec0.so" CACHE PATH "Path to vec0.so extension")
    
    target_compile_definitions(OmniNet PUBLIC OMNINET_VEC0_PATH="${OMNINET_VEC0_PATH}")
    message(STATUS "sqlite-vec enabled: ${OMNINET_VEC0_PATH}")
else()
    message(STATUS "sqlite-vec vector database DISABLED")
endif()

# Define version macro
target_compile_definitions(OmniNet PUBLIC
    OMNINET_VERSION_MAJOR=${DETECTED_MAJOR}
    OMNINET_VERSION_MINOR=${DETECTED_MINOR}
    OMNINET_VERSION_PATCH=${DETECTED_PATCH}
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

# Optional components — ON for standalone builds, OFF when OmniNet is
# pulled in as a dependency (FetchContent/add_subdirectory), so consumers
# only build the static library. Consumers can opt back in explicitly.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_OMNINET_COMPONENT_DEFAULT ON)
else()
    set(_OMNINET_COMPONENT_DEFAULT OFF)
endif()

option(OMNINET_BUILD_TESTS "Build OmniNet tests" ${_OMNINET_COMPONENT_DEFAULT})
option(OMNINET_BUILD_DEMOS "Build OmniNet demos" ${_OMNINET_COMPONENT_DEFAULT})
option(OMNINET_BUILD_BENCHMARKS "Build OmniNet benchmarks" ${_OMNINET_COMPONENT_DEFAULT})
option(OMNINET_BUILD_TOOLS "Build OmniNet CLI tools" ${_OMNINET_COMPONENT_DEFAULT})

# Enable testing
if(OMNINET_BUILD_TESTS)
    include(CTest)
    enable_testing()
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

# Build demos
if(OMNINET_BUILD_DEMOS)
    add_subdirectory(demos)
endif()

# Build benchmarks
if(OMNINET_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Build CLI tools
if(OMNINET_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ============================================================================
# cppcheck static analysis target
# ============================================================================
# Runs cppcheck only on project source (src/) and tests (tests/). Third-party
# dependencies under build/_deps/ are never scanned.
# The target reports all findings but only fails the build if cppcheck reports
# an error-level finding (warnings do not fail the build).
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    set(CPPCHECK_LOG "${CMAKE_BINARY_DIR}/cppcheck.log")
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --enable=all
            --std=c++23
            --suppress=ctuOneDefinitionRuleViolation:*tests/*
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --suppress=normalCheckLevelMaxBranches
            --suppress=checkersReport
            # The following are style/design checks that are not bugs in this codebase
            --suppress=functionStatic
            --suppress=useStlAlgorithm
            --suppress=noExplicitConstructor
            --suppress=knownConditionTrueFalse
            --suppress=constVariableReference
            --suppress=constVariablePointer
            --suppress=useInitializationList
            --suppress=nullPointerRedundantCheck:src/async/AsyncTlsSocket.cpp
            --template=gcc
            > "${CPPCHECK_LOG}" 2>&1 || true
        COMMAND ${CMAKE_COMMAND} -E cat "${CPPCHECK_LOG}"
        COMMAND grep -q ": error:" "${CPPCHECK_LOG}" && exit 1 || exit 0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck on src/ and tests/"
        VERBATIM
    )
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")
else()
    message(STATUS "cppcheck not found - 'cppcheck' target unavailable")
endif()


