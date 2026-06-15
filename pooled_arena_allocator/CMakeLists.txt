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

add_library(arena_lib arena.cpp arena.h)

enable_testing()

add_executable(TestArena ./tests/test_arena.cpp) 
target_link_libraries(TestArena PRIVATE arena_lib doctest::doctest)
target_compile_options(TestArena PRIVATE -fsanitize=address,undefined)
target_link_options(TestArena PRIVATE -fsanitize=address,undefined)
add_test(NAME ArenaUnitTests COMMAND TestArena)





