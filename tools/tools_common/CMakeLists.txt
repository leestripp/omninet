# OmniNet Tools - Common Library
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# Common library sources
set(COMMON_SOURCES
    common.cpp
    colors.cpp
    table.cpp
    config.cpp
    database.cpp
    http.cpp
    version.cpp
    password.cpp
)

# Create static library
add_library(omninet_tools_common STATIC
    ${COMMON_SOURCES}
)

# C++20
target_compile_features(omninet_tools_common PRIVATE cxx_std_20)

# Include directories (PUBLIC so dependents inherit)
target_include_directories(omninet_tools_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)

# Find OpenSSL for password hashing
find_package(OpenSSL REQUIRED)

# Link dependencies
target_link_libraries(omninet_tools_common PRIVATE
    sqlite3
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Platform-specific
if(WIN32)
    target_link_libraries(omninet_tools_common PRIVATE
        ws2_32
        crypt32
    )
endif()

# v0.6.0 fix (bug #11) — REVISED for v0.7.0:
# OMNINET_VERSION_* is added as PUBLIC compile definitions on the
# OmniNet target by the root CMakeLists.txt. However, PUBLIC
# compile definitions do NOT transitively propagate through
# static-library dependents. tools_common links against OmniNet
# internally, but tools link against tools_common — and tools
# never see OmniNet's PUBLIC compile defs. So tools_common must
# re-export them. The redefinition warnings previously came from
# two blocks (an if(DEFINED) and an unconditional fallback) both
# setting the same macros AND colliding with OmniNet's PUBLIC
# exports via transitive includes. Here we set them ONCE, using
# the root project-level PROJECT_VERSION_* variables (set by
# project(OmniNet VERSION ...) in the root CMakeLists.txt).
target_compile_definitions(omninet_tools_common PUBLIC
    OMNINET_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    OMNINET_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    OMNINET_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Build type - always define as a string
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(omninet_tools_common PUBLIC OMNINET_BUILD_TYPE="debug")
else()
    target_compile_definitions(omninet_tools_common PUBLIC OMNINET_BUILD_TYPE="release")
endif()
