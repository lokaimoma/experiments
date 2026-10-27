cmake_minimum_required(VERSION 3.14)
project(experiments)

enable_testing()

add_subdirectory(pooled_arena_allocator)
add_subdirectory(hashtable)
add_subdirectory(tga_image_header_reader)
add_subdirectory(custom_pak_archive_parser_index_reader)
