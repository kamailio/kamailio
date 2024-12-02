cmake_minimum_required(VERSION 3.10)

project(matrix)

file(GLOB SRC_FILES "*.c")

add_library(matrix SHARED ${SRC_FILES})
target_link_libraries(matrix PRIVATE common_modules kamailio)

target_link_libraries(matrix PRIVATE srdb1)
