cmake_minimum_required(VERSION 3.10)

project(misctest)

file(GLOB SRC_FILES "*.c")

add_library(misctest SHARED ${SRC_FILES})
target_link_libraries(misctest PRIVATE kamailio)
