# OmniNet Tools - Main CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# Build all CLI tools

# Include common library
add_subdirectory(tools_common)

# Tools directory (for includes)
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Tool: omninet-status
# ============================================================================
add_executable(omninet_status
    omninet-status/main.cpp
)
target_compile_features(omninet_status PRIVATE cxx_std_20)
target_include_directories(omninet_status PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_status PUBLIC
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-health
# ============================================================================
add_executable(omninet_health
    omninet-health/main.cpp
)
target_compile_features(omninet_health PRIVATE cxx_std_20)
target_include_directories(omninet_health PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_health PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-log
# ============================================================================
add_executable(omninet_log
    omninet-log/main.cpp
)
target_compile_features(omninet_log PRIVATE cxx_std_20)
target_include_directories(omninet_log PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_log PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-firewall
# ============================================================================
add_executable(omninet_firewall
    omninet-firewall/main.cpp
)
target_compile_features(omninet_firewall PRIVATE cxx_std_20)
target_include_directories(omninet_firewall PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_firewall PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-users
# ============================================================================
add_executable(omninet_users
    omninet-users/main.cpp
)
target_compile_features(omninet_users PRIVATE cxx_std_20)
target_include_directories(omninet_users PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_users PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-db
# ============================================================================
add_executable(omninet_db
    omninet-db/main.cpp
)
target_compile_features(omninet_db PRIVATE cxx_std_20)
target_include_directories(omninet_db PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_db PRIVATE
    omninet_tools_common
    sqlite3
    Threads::Threads
)

# ============================================================================
# Tool: omninet-certs
# ============================================================================
add_executable(omninet_certs
    omninet-certs/main.cpp
)
target_compile_features(omninet_certs PRIVATE cxx_std_20)
target_include_directories(omninet_certs PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_certs PRIVATE
    omninet_tools_common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# ============================================================================
# Tool: omninet-ctl
# ============================================================================
add_executable(omninet_ctl
    omninet-ctl/main.cpp
)
target_compile_features(omninet_ctl PRIVATE cxx_std_20)
target_include_directories(omninet_ctl PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_ctl PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-mon
# ============================================================================
add_executable(omninet_mon
    omninet-mon/main.cpp
)
target_compile_features(omninet_mon PRIVATE cxx_std_20)
target_include_directories(omninet_mon PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_mon PRIVATE
    omninet_tools_common
    Threads::Threads
)

# ============================================================================
# Tool: omninet-init
# ============================================================================
add_executable(omninet_init
    omninet-init/main.cpp
)
target_compile_features(omninet_init PRIVATE cxx_std_20)
target_include_directories(omninet_init PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TOOLS_DIR}
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)
target_link_libraries(omninet_init PRIVATE
    omninet_tools_common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# ============================================================================
# Group all tools
# ============================================================================
add_custom_target(tools ALL
    DEPENDS
        omninet_status
        omninet_health
        omninet_log
        omninet_firewall
        omninet_users
        omninet_db
        omninet_certs
        omninet_ctl
        omninet_mon
        omninet_init
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS
    omninet_status
    omninet_health
    omninet_log
    omninet_firewall
    omninet_users
    omninet_db
    omninet_certs
    omninet_ctl
    omninet_mon
    omninet_init
    RUNTIME DESTINATION bin
)

# Create symlinks without the underscore for user convenience
# (optional - can be done post-install)
