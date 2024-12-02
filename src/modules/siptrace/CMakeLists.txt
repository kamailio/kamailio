cmake_minimum_required(VERSION 3.10)

project(siptrace)

file(GLOB SRC_FILES "*.c")

add_library(siptrace SHARED ${SRC_FILES})

target_link_libraries(siptrace PRIVATE kamailio)
target_link_libraries(siptrace PRIVATE srdb1)
