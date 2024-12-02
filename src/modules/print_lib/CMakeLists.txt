cmake_minimum_required(VERSION 3.10)

project(print_lib)

file(GLOB SRC_FILES "*.c")

add_library(print_lib SHARED ${SRC_FILES})
# lib_print is the name of the library that we are linking against
# (src/lib/print)
# if `print` it will pick the module one
target_link_libraries(print_lib PRIVATE kamailio lib_print)
