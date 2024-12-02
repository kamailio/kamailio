cmake_minimum_required(VERSION 3.10)

project(statsc)

file(GLOB SRC_FILES "*.c")

add_library(statsc SHARED ${SRC_FILES})
target_link_libraries(statsc PRIVATE kamailio)
