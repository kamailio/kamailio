cmake_minimum_required(VERSION 3.10)

project(lost)

file(GLOB SRC_FILES "*.c")

add_library(lost SHARED ${SRC_FILES})
target_link_libraries(lost PRIVATE common_modules kamailio)
