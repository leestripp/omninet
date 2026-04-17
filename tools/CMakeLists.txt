# OmniNet Tools - Main CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# Build all CLI tools

# Include common library
add_subdirectory(tools_common)

# Tools directory (for includes)
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Helper macro to add a tool with standard settings
macro(add_omninet_tool name)
    add_executable(${name} ${name}/main.cpp)
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${TOOLS_DIR}
        ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
    )
endmacro()

# ============================================================================
# Tool: omninet-status
# ============================================================================
add_omninet_tool(omninet-status)
target_link_libraries(omninet-status PUBLIC
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-log
# ============================================================================
add_omninet_tool(omninet-log)
target_link_libraries(omninet-log PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-firewall
# ============================================================================
add_omninet_tool(omninet-firewall)
target_link_libraries(omninet-firewall PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-users
# ============================================================================
add_omninet_tool(omninet-users)
target_link_libraries(omninet-users PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-db
# ============================================================================
add_omninet_tool(omninet-db)
target_link_libraries(omninet-db PRIVATE
    omninet_tools_common
    sqlite3
    Threads::Threads
)

# ============================================================================
# Tool: omninet-certs
# ============================================================================
add_omninet_tool(omninet-certs)
target_link_libraries(omninet-certs PRIVATE
    omninet_tools_common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# ============================================================================
# Tool: omninet-ctl
# ============================================================================
add_omninet_tool(omninet-ctl)
target_link_libraries(omninet-ctl PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-init
# ============================================================================
add_omninet_tool(omninet-init)
target_link_libraries(omninet-init PRIVATE
    omninet_tools_common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# ============================================================================
# Tool: omninet-health
# ============================================================================
add_omninet_tool(omninet-health)
target_link_libraries(omninet-health PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-mon
# ============================================================================
add_omninet_tool(omninet-mon)
target_link_libraries(omninet-mon PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Group all tools
# ============================================================================
add_custom_target(tools ALL
    DEPENDS
        omninet-status
        omninet-log
        omninet-firewall
        omninet-users
        omninet-db
        omninet-certs
        omninet-ctl
        omninet-init
        omninet-health
        omninet-mon
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS
    omninet-status
    omninet-log
    omninet-firewall
    omninet-users
    omninet-db
    omninet-certs
    omninet-ctl
    omninet-init
    omninet-health
    omninet-mon
    RUNTIME DESTINATION bin
)
