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

# Config-specific test executable
add_executable(test_config
    test_config.cpp
)

# Link with OmniNet and dependencies
target_link_libraries(omnet_test PRIVATE
    OmniNet
    GTest::GTest
    pthread
)

target_link_libraries(test_config PRIVATE
    OmniNet
    pthread
)

# Enable C++20
target_compile_features(omnet_test PRIVATE cxx_std_20)
target_compile_features(test_config PRIVATE cxx_std_20)

# Include directories - use parent source dir
target_include_directories(omnet_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(test_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Copy test resources (if directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endif()

# Register tests
enable_testing()
add_test(NAME OmniNetTests COMMAND omnet_test)
add_test(NAME ConfigTests COMMAND test_config)
