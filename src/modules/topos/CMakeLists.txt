cmake_minimum_required(VERSION 3.10)

project(topos)

file(GLOB SRC_FILES "*.c")

add_library(topos SHARED ${SRC_FILES})

target_link_libraries(topos PRIVATE kamailio)
