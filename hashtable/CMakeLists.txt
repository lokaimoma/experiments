cmake_minimum_required(VERSION 3.14)
project(hashtable CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(htmap_lib table.cpp table.h)
target_link_libraries(htmap_lib PUBLIC arena_lib)
