cmake_minimum_required(VERSION 3.10)

project(call_obj)

file(GLOB SRC_FILES "*.c")

add_library(call_obj SHARED ${SRC_FILES})
target_link_libraries(call_obj PRIVATE kamailio)
