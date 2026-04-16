# OmniNet Demos
cmake_minimum_required(VERSION 3.16)

project(OmniNetDemos)

# Basic HTTP server demo
add_executable(http_server_demo
    http_server.cpp
)

target_link_libraries(http_server_demo PRIVATE OmniNet)
target_include_directories(http_server_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# HTTPS server demo
add_executable(https_server_demo
    https_server.cpp
)

target_link_libraries(https_server_demo PRIVATE OmniNet)
target_include_directories(https_server_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# WebSocket echo demo
add_executable(websocket_echo_demo
    websocket_echo.cpp
)

target_link_libraries(websocket_echo_demo PRIVATE OmniNet)
target_include_directories(websocket_echo_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Static file server demo
add_executable(static_files_demo
    static_files.cpp
)

target_link_libraries(static_files_demo PRIVATE OmniNet)
target_include_directories(static_files_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Dashboard demo
add_executable(dashboard_demo
    dashboard.cpp
)

target_link_libraries(dashboard_demo PRIVATE OmniNet)
target_include_directories(dashboard_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
