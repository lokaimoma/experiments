cmake_minimum_required(VERSION 3.14)
project(pooled_arena_allocator CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest
  GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(doctest)

add_library(arena_lib arena.cpp arena.h objectpool.h)
target_include_directories(arena_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(TestArena ./tests/test_arena.cpp) 
target_link_libraries(TestArena PRIVATE arena_lib doctest::doctest)
target_compile_options(TestArena PRIVATE -fsanitize=address,undefined)
target_link_options(TestArena PRIVATE -fsanitize=address,undefined)

add_executable(TestObjectPool ./tests/test_objectpool.cpp) 
target_link_libraries(TestObjectPool PRIVATE arena_lib doctest::doctest)
target_compile_options(TestObjectPool PRIVATE -fsanitize=address,undefined)
target_link_options(TestObjectPool PRIVATE -fsanitize=address,undefined)

add_test(NAME ArenaUnitTests COMMAND TestArena)
add_test(NAME ObjectPoolUnitTests COMMAND TestObjectPool)
