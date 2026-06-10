cmake_minimum_required(VERSION 3.16)

# Add local cmake modules to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

project(OmniNet VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

# C++23 required (Phase 1 of async refactor)
# Compiler requirements: GCC 13+ / Clang 17+ / MSVC 19.36+
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
    
    # Vector (sqlite-vec)
    src/vector/VectorStore.cpp
    
    # Thread Pool (Phase 2)
    src/thread/ThreadPool.cpp
    
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
# Use find_library/find_path for compatibility with standard zlib1g-dev package
find_path(ZLIB_INCLUDE_DIR NAMES zlib.h)
find_library(ZLIB_LIBRARY NAMES z)
if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
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

# ============================================================================
# Cppcheck Static Analysis
# ============================================================================
find_package(Cppcheck QUIET)
if(Cppcheck_FOUND)
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")
    
    # Create directory for cppcheck reports
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cppcheck")
    
    # Helper macro to add cppcheck to a target
    macro(add_cppcheck_target TARGET_NAME)
        add_custom_target(${TARGET_NAME}
            COMMAND ${CPPCHECK_EXECUTABLE}
                --std=c++23
                --enable=all
                --inconclusive
                --verbose
                --quiet
                --output-file=${CMAKE_BINARY_DIR}/cppcheck/${TARGET_NAME}.xml
                --xml
                --template=file:{file}:{line}:{column}:{severity}:{id}:{message}
                ${ARGN}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running cppcheck static analysis for ${TARGET_NAME}"
        )
        # Also create a console output version
        add_custom_target(${TARGET_NAME}_console
            COMMAND ${CPPCHECK_EXECUTABLE}
                --std=c++23
                --enable=all
                --inconclusive
                --quiet
                --template=file:{file}:{line}:{column}:{severity}:{id}:{message}
                ${ARGN}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running cppcheck for ${TARGET_NAME} (console output)"
        )
    endmacro()
    
    # Analyze library source files
    add_cppcheck_target(cppcheck_lib
        src/
    )
    
    # Analyze test source files
    add_cppcheck_target(cppcheck_tests
        tests/
    )
    
    # Analyze demo source files
    add_cppcheck_target(cppcheck_demos
        demos/
    )
    
    # Analyze tool source files
    add_cppcheck_target(cppcheck_tools
        tools/
    )
    
    # Master target to run all cppcheck analyses
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "Running all cppcheck analyses..."
        COMMAND ${CMAKE_COMMAND} --build . --target cppcheck_lib_console
        COMMAND ${CMAKE_COMMAND} --build . --target cppcheck_tests_console
        COMMAND ${CMAKE_COMMAND} --build . --target cppcheck_demos_console
        COMMAND ${CMAKE_COMMAND} --build . --target cppcheck_tools_console
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running all cppcheck analyses"
    )
    
    message(STATUS "cppcheck targets created: cppcheck, cppcheck_lib, cppcheck_tests, cppcheck_demos, cppcheck_tools")
else()
    message(STATUS "cppcheck not found - static analysis disabled (install cppcheck to enable)")
endif()
