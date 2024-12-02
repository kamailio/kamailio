cmake_minimum_required(VERSION 3.10)

project(sworker)

file(GLOB SRC_FILES "*.c")

add_library(sworker SHARED ${SRC_FILES})
target_link_libraries(sworker PRIVATE common_modules kamailio)
