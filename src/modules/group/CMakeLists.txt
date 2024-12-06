cmake_minimum_required(VERSION 3.10)

project(group)

file(GLOB SRC_FILES "*.c")

add_library(group SHARED ${SRC_FILES})

target_link_libraries(group PRIVATE kamailio)
