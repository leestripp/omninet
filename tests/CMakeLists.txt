# OmniNet Tests
cmake_minimum_required(VERSION 3.16)

# Note: Don't call project() here - use parent project variables

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

# WebSocket broadcast tests (v0.7.0 Phase 11b)
add_executable(test_websocket_broadcast
    test_websocket_broadcast.cpp
)

# WebSocket end-to-end echo test (v0.7.0, prompt.txt regression test)
add_executable(test_websocket_echo
    test_websocket_echo.cpp
)

# SSE streaming tests (v0.8.3 MonkeyByte Bug 1)
add_executable(test_sse_streaming
    test_sse_streaming.cpp
)

# StreamingResponse tests (v0.9.0 prompt.txt A2A streaming)
add_executable(test_streaming_response
    test_streaming_response.cpp
)

# HTTP Client tests (v0.8.0 Phase 1)
add_executable(test_http_client
    test_http_client.cpp
)

# XDG Base Directory tests
add_executable(test_xdg_paths
    test_xdg_paths.cpp
)

# UDP socket tests (game networking)
add_executable(test_udp_socket
    test_udp_socket.cpp
)

# BitStream serialization tests (game networking)
add_executable(test_bitstream
    test_bitstream.cpp
)

# GameConnection protocol tests (game networking)
add_executable(test_game_connection
    test_game_connection.cpp
)

# GameServer/GameClient end-to-end tests (game networking)
add_executable(test_game_network
    test_game_network.cpp
)

# Delta snapshot compression tests (game networking)
add_executable(test_delta_snapshot
    test_delta_snapshot.cpp
)

# DTLS datagram socket tests
add_executable(test_dtls_socket
    test_dtls_socket.cpp
)

# Link with OmniNet and dependencies
target_link_libraries(omnet_test PRIVATE OmniNet pthread)
target_link_libraries(test_buffer_protection PRIVATE OmniNet pthread)

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

target_link_libraries(test_websocket_broadcast PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_sse_streaming PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_streaming_response PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_websocket_echo PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_http_client PRIVATE
    OmniNet
    pthread
)

target_link_libraries(test_xdg_paths PRIVATE
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

target_compile_features(test_sse_streaming PRIVATE cxx_std_23)
target_compile_features(test_streaming_response PRIVATE cxx_std_23)

target_compile_features(test_websocket_broadcast PRIVATE cxx_std_23)
target_compile_features(test_websocket_echo PRIVATE cxx_std_23)
target_compile_features(test_http_client PRIVATE cxx_std_23)
target_compile_features(test_xdg_paths PRIVATE cxx_std_23)

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
target_include_directories(test_http_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_xdg_paths PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(test_streaming_response PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Note: zlib is handled by the main OmniNet library
# If compression is enabled, the library will link it

# Register all test executables with CTest
add_test(NAME omnet_test COMMAND omnet_test)
add_test(NAME test_buffer_protection COMMAND test_buffer_protection)
add_test(NAME test_config COMMAND test_config)
add_test(NAME test_token COMMAND test_token)
add_test(NAME test_thread_pool COMMAND test_thread_pool)
add_test(NAME test_async_task COMMAND test_async_task)
add_test(NAME test_io_context COMMAND test_io_context)
add_test(NAME test_async_server COMMAND test_async_server)
add_test(NAME test_async_database COMMAND test_async_database)
add_test(NAME test_async_logger COMMAND test_async_logger)
add_test(NAME test_async_tls COMMAND test_async_tls)
add_test(NAME test_expected COMMAND test_expected)
add_test(NAME test_async_file COMMAND test_async_file)
add_test(NAME test_http_connection COMMAND test_http_connection)
add_test(NAME test_sync_http_server COMMAND test_sync_http_server)
add_test(NAME test_http_response COMMAND test_http_response)
add_test(NAME test_websocket_broadcast COMMAND test_websocket_broadcast)
add_test(NAME test_websocket_echo COMMAND test_websocket_echo)
add_test(NAME test_sse_streaming COMMAND test_sse_streaming)
add_test(NAME test_streaming_response COMMAND test_streaming_response)
add_test(NAME test_http_client COMMAND test_http_client)
add_test(NAME test_xdg_paths COMMAND test_xdg_paths)
target_link_libraries(test_udp_socket PRIVATE OmniNet pthread)
target_link_libraries(test_bitstream PRIVATE OmniNet pthread)
target_link_libraries(test_game_connection PRIVATE OmniNet pthread)
target_link_libraries(test_game_network PRIVATE OmniNet pthread)
add_test(NAME test_udp_socket COMMAND test_udp_socket)
add_test(NAME test_bitstream COMMAND test_bitstream)
add_test(NAME test_game_connection COMMAND test_game_connection)
add_test(NAME test_game_network COMMAND test_game_network)
target_link_libraries(test_delta_snapshot PRIVATE OmniNet pthread)
add_test(NAME test_delta_snapshot COMMAND test_delta_snapshot)
target_link_libraries(test_dtls_socket PRIVATE OmniNet pthread)
add_test(NAME test_dtls_socket COMMAND test_dtls_socket)
