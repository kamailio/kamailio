cmake_minimum_required(VERSION 3.10)

project(maxfwd)

file(GLOB SRC_FILES "*.c")

add_library(maxfwd SHARED ${SRC_FILES})
target_link_libraries(maxfwd PRIVATE kamailio)
