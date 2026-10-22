cmake_minimum_required(VERSION 3.14)
project(experiments)

enable_testing()

add_subdirectory(pooled_arena_allocator)
add_subdirectory(hashtable)
add_subdirectory(http1.1_parser)
add_subdirectory(tga_image_header_parser)
