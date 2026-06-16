cmake_minimum_required(VERSION 3.14)
project(hashtable CXX)

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

add_library(htmap_lib table.cpp table.h hmap.h hmaphasher.h)
target_link_libraries(htmap_lib PUBLIC arena_lib)

add_executable(TestTable ./tests/test_table.cpp) 
target_link_libraries(TestTable PRIVATE htmap_lib doctest::doctest)
target_compile_options(TestTable PRIVATE -fsanitize=address,undefined)
target_link_options(TestTable PRIVATE -fsanitize=address,undefined)

add_executable(TestHMap ./tests/test_hmap.cpp) 
target_link_libraries(TestHMap PRIVATE htmap_lib doctest::doctest)
target_compile_options(TestHMap PRIVATE -fsanitize=address,undefined)
target_link_options(TestHMap PRIVATE -fsanitize=address,undefined)

add_test(NAME TableUnitTests COMMAND TestTable)
add_test(NAME HMapUnitTests COMMAND TestHMap)

