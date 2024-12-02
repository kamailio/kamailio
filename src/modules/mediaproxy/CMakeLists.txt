cmake_minimum_required(VERSION 3.10)

project(mediaproxy)

file(GLOB SRC_FILES "*.c")

add_library(mediaproxy SHARED ${SRC_FILES})
target_link_libraries(mediaproxy PRIVATE kamailio)
