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
