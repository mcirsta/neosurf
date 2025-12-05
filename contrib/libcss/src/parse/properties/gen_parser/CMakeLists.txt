cmake_minimum_required(VERSION 3.20)
project(gen_parser C)

add_executable(gen_parser
    ../css_property_parser_gen.c
)

target_include_directories(gen_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/contrib/libcss/src
)

