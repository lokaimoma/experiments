cmake_minimum_required(VERSION 3.14)
project(http1_1parser CXX)

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

add_library(http1_1parser_lib parser.cpp)
target_compile_options(http1_1parser_lib PRIVATE -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined)
target_link_options(http1_1parser_lib PRIVATE -fsanitize=address,undefined)
target_include_directories(http1_1parser_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(TestParser ./tests/test_parser.cpp) 
target_link_libraries(TestParser PRIVATE http1_1parser_lib doctest::doctest)
target_compile_options(TestParser PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_options(TestParser PRIVATE -fsanitize=address,undefined)

add_test(NAME Http1_1ParserUnitTests COMMAND TestParser)

