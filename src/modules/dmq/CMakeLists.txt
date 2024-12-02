cmake_minimum_required(VERSION 3.10)

project(dmq)

file(GLOB SRC_FILES "*.c")

add_library(dmq SHARED ${SRC_FILES})

target_link_libraries(dmq PRIVATE kamailio)
target_link_libraries(dmq PRIVATE srdb1)
