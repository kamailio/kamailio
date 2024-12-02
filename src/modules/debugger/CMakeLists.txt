cmake_minimum_required(VERSION 3.10)

project(debugger)

file(GLOB SRC_FILES "*.c")

add_library(debugger SHARED ${SRC_FILES})
target_link_libraries(debugger PRIVATE common_modules kamailio)
