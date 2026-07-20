cmake_minimum_required(VERSION 3.14)
project(http1dot1impl CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(server1dot1_lib server.cpp http1_parser.cpp str_utils.cpp)
target_include_directories(server1dot1_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(server1dot1example main.cpp) 
target_link_libraries(server1dot1example PRIVATE server1dot1_lib)

