cmake_minimum_required(VERSION 3.10)

project(exec)

file(GLOB SRC_FILES "*.c")

add_library(exec SHARED ${SRC_FILES})
target_link_libraries(exec PRIVATE kamailio)
