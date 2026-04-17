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

# Link dependencies
target_link_libraries(omninet_tools_common PRIVATE
    sqlite3
    Threads::Threads
)

# Platform-specific
if(WIN32)
    target_link_libraries(omninet_tools_common PRIVATE
        ws2_32
        crypt32
    )
endif()

# Version definitions - passed from parent project or use defaults
# These come from the root CMakeLists.txt if available
if(DEFINED PROJECT_VERSION_MAJOR)
    target_compile_definitions(omninet_tools_common PUBLIC
        OMNINET_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        OMNINET_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        OMNINET_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )
endif()

# Build type - always define as a string
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(omninet_tools_common PUBLIC OMNINET_BUILD_TYPE="debug")
else()
    target_compile_definitions(omninet_tools_common PUBLIC OMNINET_BUILD_TYPE="release")
endif()

# Fallback version macros (in case PROJECT_VERSION_MAJOR not set)
target_compile_definitions(omninet_tools_common PUBLIC
    OMNINET_VERSION_MAJOR=${OMNINET_VERSION_MAJOR}
    OMNINET_VERSION_MINOR=${OMNINET_VERSION_MINOR}
    OMNINET_VERSION_PATCH=${OMNINET_VERSION_PATCH}
)
