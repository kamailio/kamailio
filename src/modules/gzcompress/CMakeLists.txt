cmake_minimum_required(VERSION 3.10)

project(gzcompress)

file(GLOB SRC_FILES "*.c")

add_library(gzcompress SHARED ${SRC_FILES})
target_link_libraries(gzcompress PRIVATE common_modules kamailio)
