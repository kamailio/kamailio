cmake_minimum_required(VERSION 3.10)

project(keepalive)

file(GLOB SRC_FILES "*.c")

add_library(keepalive SHARED ${SRC_FILES})

target_link_libraries(keepalive PRIVATE kamailio)
target_link_libraries(keepalive PRIVATE srdb1)
