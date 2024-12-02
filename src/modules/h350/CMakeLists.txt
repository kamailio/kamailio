cmake_minimum_required(VERSION 3.10)

project(h350)

file(GLOB SRC_FILES "*.c")

add_library(h350 SHARED ${SRC_FILES})
target_link_libraries(h350 PRIVATE common_modules kamailio)
