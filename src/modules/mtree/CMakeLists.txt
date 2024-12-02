cmake_minimum_required(VERSION 3.10)

project(mtree)

file(GLOB SRC_FILES "*.c")

add_library(mtree SHARED ${SRC_FILES})
target_link_libraries(mtree PRIVATE common_modules kamailio)

target_link_libraries(mtree PRIVATE srdb1)
