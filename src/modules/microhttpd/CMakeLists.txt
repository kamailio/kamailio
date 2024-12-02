cmake_minimum_required(VERSION 3.10)

project(microhttpd)

file(GLOB SRC_FILES "*.c")

add_library(microhttpd SHARED ${SRC_FILES})
target_link_libraries(microhttpd PRIVATE common_modules kamailio)
