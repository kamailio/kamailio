cmake_minimum_required(VERSION 3.10)

project(log_custom)

file(GLOB SRC_FILES "*.c")

add_library(log_custom SHARED ${SRC_FILES})
target_link_libraries(log_custom PRIVATE kamailio)
