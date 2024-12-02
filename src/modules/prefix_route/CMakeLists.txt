cmake_minimum_required(VERSION 3.10)

project(prefix_route)

file(GLOB SRC_FILES "*.c")

add_library(prefix_route SHARED ${SRC_FILES})

target_link_libraries(prefix_route PRIVATE kamailio)
target_link_libraries(prefix_route PRIVATE srdb2)
