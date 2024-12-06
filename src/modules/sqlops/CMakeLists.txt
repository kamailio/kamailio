cmake_minimum_required(VERSION 3.10)

project(sqlops)

file(GLOB SRC_FILES "*.c")

add_library(sqlops SHARED ${SRC_FILES})

target_link_libraries(sqlops PRIVATE kamailio)
