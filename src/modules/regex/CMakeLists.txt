cmake_minimum_required(VERSION 3.10)

project(regex)

file(GLOB SRC_FILES "*.c")

add_library(regex SHARED ${SRC_FILES})
target_link_libraries(regex PRIVATE common_modules kamailio)
