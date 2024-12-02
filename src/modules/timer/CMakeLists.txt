cmake_minimum_required(VERSION 3.10)

project(timer)

file(GLOB SRC_FILES "*.c")

add_library(timer SHARED ${SRC_FILES})
target_link_libraries(timer PRIVATE kamailio)
