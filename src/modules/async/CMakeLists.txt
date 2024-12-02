cmake_minimum_required(VERSION 3.10)

project(async)

file(GLOB SRC_FILES "*.c")

add_library(async SHARED ${SRC_FILES})
target_link_libraries(async PRIVATE kamailio)
