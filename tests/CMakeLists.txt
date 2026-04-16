# OmniNet Tests
cmake_minimum_required(VERSION 3.16)

project(OmniNetTests)

# Find unittest or use GoogleTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, tests will be minimal")
endif()

# Test executable - only compile test_main.cpp
add_executable(omnet_test
    test_main.cpp
)

# Link with OmniNet and dependencies
target_link_libraries(omnet_test PRIVATE
    OmniNet
    GTest::GTest
    pthread
)

# Enable C++20
target_compile_features(omnet_test PRIVATE cxx_std_20)

# Include directories
target_include_directories(omnet_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Copy test resources (if directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endif()

# Register tests
enable_testing()
add_test(NAME OmniNetTests COMMAND omnet_test)