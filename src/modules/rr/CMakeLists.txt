cmake_minimum_required(VERSION 3.10)

project(rr)

file(GLOB SRC_FILES "*.c")

add_library(rr SHARED ${SRC_FILES})
target_link_libraries(rr PRIVATE common common_modules kamailio)
