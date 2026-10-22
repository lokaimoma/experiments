cmake_minimum_required(VERSION 3.14)
project(cpkg_reader CXX)

enable_testing()

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

add_library(libcpkg_reader parser.cpp)
target_compile_options(libcpkg_reader PRIVATE -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined)
target_link_options(libcpkg_reader PRIVATE -fsanitize=address,undefined)
target_include_directories(libcpkg_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cpkg_reader_mtester main.cpp)
target_link_libraries(cpkg_reader_mtester PRIVATE libcpkg_reader)
target_compile_options(cpkg_reader_mtester PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_options(cpkg_reader_mtester PRIVATE -fsanitize=address,undefined)
