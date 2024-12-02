cmake_minimum_required(VERSION 3.10)

project(tsilo)

file(GLOB SRC_FILES "*.c")

add_library(tsilo SHARED ${SRC_FILES})
target_link_libraries(tsilo PRIVATE common_modules kamailio)
