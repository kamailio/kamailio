cmake_minimum_required(VERSION 3.10)

project(blst)

file(GLOB SRC_FILES "*.c")

add_library(blst SHARED ${SRC_FILES})
target_link_libraries(blst PRIVATE kamailio)
