cmake_minimum_required(VERSION 3.14)
project(tga_image_header_parser CXX)

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

add_library(tga_image_header_parser_lib parser.cpp)
target_compile_options(tga_image_header_parser_lib PRIVATE -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined)
target_link_options(tga_image_header_parser_lib PRIVATE -fsanitize=address,undefined)
target_include_directories(tga_image_header_parser_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(TestTGAParser ./tests/test_parser.cpp) 
target_link_libraries(TestTGAParser PRIVATE tga_image_header_parser_lib doctest::doctest)
target_compile_options(TestTGAParser PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_options(TestTGAParser PRIVATE -fsanitize=address,undefined)

add_test(NAME Http1_1ParserUnitTests COMMAND TestParser)

