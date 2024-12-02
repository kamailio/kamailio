cmake_minimum_required(VERSION 3.10)

project(lrkproxy)

file(GLOB SRC_FILES "*.c")

add_library(lrkproxy SHARED ${SRC_FILES})
target_link_libraries(lrkproxy PRIVATE kamailio)
