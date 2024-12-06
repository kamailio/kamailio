cmake_minimum_required(VERSION 3.10)

project(secfilter)

file(GLOB SRC_FILES "*.c")

add_library(secfilter SHARED ${SRC_FILES})

target_link_libraries(secfilter PRIVATE kamailio)
