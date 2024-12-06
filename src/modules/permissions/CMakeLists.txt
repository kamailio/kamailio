cmake_minimum_required(VERSION 3.10)

project(permissions)

file(GLOB SRC_FILES "*.c")

add_library(permissions SHARED ${SRC_FILES})

target_link_libraries(permissions PRIVATE kamailio)
