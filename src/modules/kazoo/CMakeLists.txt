cmake_minimum_required(VERSION 3.10)

project(kazoo)

file(GLOB SRC_FILES "*.c")

add_library(kazoo SHARED ${SRC_FILES})
target_link_libraries(kazoo PRIVATE common_modules kamailio)

target_link_libraries(kazoo PRIVATE srdb1)
