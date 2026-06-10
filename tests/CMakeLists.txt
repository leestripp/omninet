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

# Enable C++23
target_compile_features(omnet_test PRIVATE cxx_std_23)
target_compile_features(test_buffer_protection PRIVATE cxx_std_23)
target_compile_features(test_config PRIVATE cxx_std_23)
target_compile_features(test_token PRIVATE cxx_std_23)
target_compile_features(test_thread_pool PRIVATE cxx_std_23)

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

# Note: zlib is handled by the main OmniNet library
# If compression is enabled, the library will link it