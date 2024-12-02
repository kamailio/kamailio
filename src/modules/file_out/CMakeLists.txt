cmake_minimum_required(VERSION 3.10)

project(file_out)

file(GLOB SRC_FILES "*.c")

add_library(file_out SHARED ${SRC_FILES})
target_link_libraries(file_out PRIVATE kamailio)
