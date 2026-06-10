# OmniNet Benchmarks
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# Logger throughput benchmark
add_executable(logger_throughput_bench
    logger_throughput_bench.cpp
)
target_link_libraries(logger_throughput_bench PRIVATE OmniNet Threads::Threads)
target_include_directories(logger_throughput_bench PRIVATE ${CMAKE_SOURCE_DIR}/src)

# std::expected overhead benchmark
add_executable(expected_overhead_bench
    expected_overhead_bench.cpp
)
target_link_libraries(expected_overhead_bench PRIVATE OmniNet)
target_include_directories(expected_overhead_bench PRIVATE ${CMAKE_SOURCE_DIR}/src)

# ThreadPool throughput benchmark
add_executable(thread_pool_bench
    thread_pool_bench.cpp
)
target_link_libraries(thread_pool_bench PRIVATE OmniNet)
target_include_directories(thread_pool_bench PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Coroutine overhead benchmark
add_executable(coroutine_overhead_bench
    coroutine_overhead_bench.cpp
)
target_link_libraries(coroutine_overhead_bench PRIVATE OmniNet)
target_include_directories(coroutine_overhead_bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
