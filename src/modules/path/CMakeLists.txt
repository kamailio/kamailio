cmake_minimum_required(VERSION 3.10)

project(path)

file(GLOB SRC_FILES "*.c")

add_library(path SHARED ${SRC_FILES})
target_link_libraries(path PRIVATE kamailio)
