cmake_minimum_required(VERSION 3.10)

project(call_control)

file(GLOB SRC_FILES "*.c")

add_library(call_control SHARED ${SRC_FILES})
target_link_libraries(call_control PRIVATE common_modules kamailio)
