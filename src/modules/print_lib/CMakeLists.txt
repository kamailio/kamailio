cmake_minimum_required(VERSION 3.10)

project(print_lib)

file(GLOB SRC_FILES "*.c")

add_library(print_lib SHARED ${SRC_FILES})
target_link_libraries(print_lib PRIVATE common_modules kamailio)
