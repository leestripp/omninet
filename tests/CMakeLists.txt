# OmniNet Tests
cmake_minimum_required(VERSION 3.16)

# Note: Don't call project() here - use parent project variables
# Find unittest or use GoogleTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, tests will be minimal")
endif()

# Comprehensive test executable - test_all.cpp has all tests
add_executable(omnet_test
    test_all.cpp
)

# Buffer overflow protection test
add_executable(test_buffer_protection
    test_buffer_protection.cpp
)

# Config-specific test executable
add_executable(test_config
    test_config.cpp
)

# Token system tests
add_executable(test_token
    test_token.cpp
)

# ThreadPool tests (Phase 2)
add_executable(test_thread_pool
    test_thread_pool.cpp
)

# Async Task tests (Phase 3)
add_executable(test_async_task
    test_async_task.cpp
)

# I/O Context tests (Phase 4)
add_executable(test_io_context
    test_io_context.cpp
)

# AsyncServer tests (Phase 5)
add_executable(test_async_server
    test_async_server.cpp
)

# AsyncDatabase tests (Phase 6)
add_executable(test_async_database
    test_async_database.cpp
)

# AsyncLogger tests (Phase 8a)
add_executable(test_async_logger
    test_async_logger.cpp
)

# AsyncTlsSocket tests (Phase 8b)
add_executable(test_async_tls
    test_async_tls.cpp
)

# std::expected migration tests (Phase 8c)
add_executable(test_expected
    test_expected.cpp
)

# AsyncFile tests (Phase 8d)
add_executable(test_async_file
    test_async_file.cpp
)

# HTTP Connection tests (v0.6.0 - chunked transfer-encoding fix)
add_executable(test_http_connection
    test_http_connection.cpp
)

# HTTP Server sync tests (v0.6.0 - Phase 10d, zero coverage before this file)
add_executable(test_sync_http_server
    test_sync_http_server.cpp
)

# HTTP Response tests
add_executable(test_http_response
    test_http_response.cpp
)

# Link with OmniNet and dependencies
target_link_libraries(omnet_test PRIVATE OmniNet pthread)
target_link_libraries(test_buffer_protection PRIVATE OmniNet pthread)
if(GTest_FOUND)
    target_link_libraries(omnet_test PRIVATE GTest::GTest)
endif()

target_link_libraries(test_config PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_token PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_thread_pool PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_task PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_io_context PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_server PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_database PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_logger PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_tls PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_expected PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_async_file PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_http_connection PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_sync_http_server PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_http_response PRIVATE
    OmniNet
    pthread
)

# Enable C++23
target_compile_features(omnet_test PRIVATE cxx_std_23)
target_compile_features(test_buffer_protection PRIVATE cxx_std_23)
target_compile_features(test_config PRIVATE cxx_std_23)
target_compile_features(test_token PRIVATE cxx_std_23)
target_compile_features(test_thread_pool PRIVATE cxx_std_23)
target_compile_features(test_async_task PRIVATE cxx_std_23)
target_compile_features(test_io_context PRIVATE cxx_std_23)
target_compile_features(test_async_server PRIVATE cxx_std_23)
target_compile_features(test_async_database PRIVATE cxx_std_23)
target_compile_features(test_async_logger PRIVATE cxx_std_23)
target_compile_features(test_async_tls PRIVATE cxx_std_23)
target_compile_features(test_expected PRIVATE cxx_std_23)
target_compile_features(test_async_file PRIVATE cxx_std_23)
target_compile_features(test_http_connection PRIVATE cxx_std_23)
target_compile_features(test_sync_http_server PRIVATE cxx_std_23)
target_compile_features(test_http_response PRIVATE cxx_std_23)

# Include directories - use parent source dir
target_include_directories(omnet_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_buffer_protection PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_token PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_thread_pool PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_task PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_io_context PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_database PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_logger PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_tls PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_expected PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_async_file PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Note: zlib is handled by the main OmniNet library
# If compression is enabled, the library will link it