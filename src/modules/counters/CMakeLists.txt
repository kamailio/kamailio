cmake_minimum_required(VERSION 3.10)

project(counters)

file(GLOB SRC_FILES "*.c")

add_library(counters SHARED ${SRC_FILES})
target_link_libraries(counters PRIVATE kamailio)

# TODO: Also install kamcmd binary
